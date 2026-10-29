#include <boost/asio.hpp>
#include <string>
#include <string_view>
#include <iostream>

#include "request.hpp"
#include "response.hpp"
#include "connectionManager.hpp"

#include "common.hpp"
#include "config.hpp"

using boost::asio::ip::tcp;

boost::asio::awaitable<bool> writeResponse(tcp::socket &socket, Response *response)
{
	std::cout<<"prepare response"<<std::endl;
	boost::asio::streambuf sResponse;
	std::ostream oStream(&sResponse);
	response->setContentLength();
	response->toStream(oStream);
	std::cout<<"response prepared"<<std::endl;
	bool res=true;
	boost::system::error_code ec;
	co_await boost::asio::async_write(socket, sResponse, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
	if(ec)
	{
		std::cout<<"Error on sent response: "<<ec.message()<<std::endl;
		res=false;
	}
	std::cout<<"response sent"<<std::endl;
	delete response;
	co_return res;
}

boost::asio::awaitable<ReadResult> readRequest(tcp::socket &socket, boost::asio::streambuf &buffer, Request *request)
{
	bool timedout=false;
	bool headersRead=false;
	boost::asio::steady_timer timer(socket.get_executor());
	timer.expires_after(std::chrono::seconds(HEADERS_TIMEOUT));
	timer.async_wait([&socket, &timedout, &headersRead](const boost::system::error_code &ec)
		{
			if(ec||headersRead)
				return;

			timedout=true;
			socket.cancel();
		});

	boost::system::error_code ec;
	const auto headerLength=co_await boost::asio::async_read_until(socket, buffer, "\r\n\r\n", boost::asio::redirect_error(boost::asio::use_awaitable, ec));
	headersRead=true;
	timer.cancel();

	if(ec)
	{
		if(ec==boost::asio::error::not_found&&buffer.size()==HEADERS_SIZE_LIMIT)
			co_return ReadResult::HEADERS_TOO_LARGE;
		else if(ec==boost::asio::error::operation_aborted&&timedout)
		{
			if(headerLength>0)
				co_return ReadResult::TIMEOUT;
			else
			//keep-alive scenario when header reading timeout fires before
			//connection timeout - read bytes==0, reading cancelled
				co_return ReadResult::CLOSE_CONNECTION;
		}
		else
			co_return ReadResult::NETWORK_ERROR;
	}

	std::istream is{&buffer};
	if(!request->parseHeaders(is, headerLength))
		co_return ReadResult::BAD_REQUEST;

	//block for not implemented encoding types, etc
	if(request->headers()!=nullptr&&
		(request->headers()->contains("transfer-encoding")))
		co_return ReadResult::NOT_IMPLEMENTED;

	unsigned int contentLength=0;
	if(request->headers()!=nullptr&&request->headers()->contains("content-length"))
	{
		const auto tmp=request->headers()->at("content-length");
		try
		{
			size_t converted=0;
			contentLength=std::stoi(tmp, &converted);
			if(converted!=tmp.size())
				co_return ReadResult::BAD_REQUEST;
		}
		catch(const std::exception &e)
		{
			co_return ReadResult::BAD_REQUEST;
		}
	}
	if(contentLength>CONTENT_LENGTH_LIMIT)
		co_return ReadResult::PAYLOAD_TOO_LARGE;

	auto available=buffer.size();
	std::string body(contentLength, '\0');
	if(available>=contentLength)
		is.read(&body[0], contentLength);
	else
	{
		is.read(&body[0], available);
		size_t remaining=contentLength-available;
		while(remaining>0)
		{
			timedout=false;
			bool chunkRead=false;
			timer.expires_after(std::chrono::seconds(BODY_IDLE_TIMEOUT));
			timer.async_wait([&socket, &timedout, &chunkRead](const boost::system::error_code &ec)
				{
					if(ec||chunkRead)
						return;
					timedout=true;
					socket.cancel();
				});
			const auto bytes=co_await socket.async_read_some(boost::asio::buffer(body.data()+available, remaining), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
			chunkRead=true;
			timer.cancel();
			if(ec)
			{
				if(ec==boost::asio::error::operation_aborted&&timedout)
					co_return ReadResult::TIMEOUT;
				else
					co_return ReadResult::NETWORK_ERROR;
			}
			available+=bytes;
			remaining-=bytes;
		}
	}
	request->body(body);
	co_return ReadResult::OK;
}

boost::asio::awaitable<void> handleConnection(tcp::socket socket)
{
	boost::asio::steady_timer timer(socket.get_executor());

	boost::asio::streambuf sRequest(HEADERS_SIZE_LIMIT);
	bool waitNextRequest=false;

	for(;;)
	{
		bool timedout=false;
		if(waitNextRequest)
		{
			timer.expires_after(std::chrono::seconds(KEEP_ALIVE_TIMEOUT));
			timer.async_wait([&socket, &timedout](const boost::system::error_code &ec)
				{
					if(ec==boost::asio::error::operation_aborted)
						return;
					if(ec)
						return;
					std::cout<<"connection timeout\n";
					timedout=true;
					socket.close();
				});
		}
		std::cout<<"read request"<<std::endl;

		auto *request=new Request();
		const auto readResult=co_await readRequest(socket, sRequest, request);

		timer.cancel();

		if(timedout)
		{
			delete request;
			co_return;
		}

		Response *response;
		switch(readResult)
		{
			case ReadResult::NETWORK_ERROR:
			case ReadResult::CLOSE_CONNECTION:
			{
				delete request;
				boost::system::error_code ec;
				socket.shutdown(tcp::socket::shutdown_both, ec);
				socket.close(ec);
				co_return;
			}
			case ReadResult::OK:
			case ReadResult::BAD_REQUEST:
			case ReadResult::HEADERS_TOO_LARGE:
			case ReadResult::PAYLOAD_TOO_LARGE:
			case ReadResult::NOT_IMPLEMENTED:
			case ReadResult::TIMEOUT:
			{
				response=new Response();
				response->requestMethod(request->method());
				response->responseCode(static_cast<uint16_t>(readResult));
				break;
			}
		};

		if(readResult!=ReadResult::OK)
		{
			delete request;
			response->setConnectionStatus();
			co_await writeResponse(socket, response);
			co_return;
		}

		const auto currentStatus=request->parseBody();
		if(currentStatus!=200)
		{
			delete request;
			response->setConnectionStatus();
			response->responseCode(currentStatus);
			co_await writeResponse(socket, response);
			co_return;
		}

		const bool keepAlive=request->keepAlive();
		response->keepAlive(keepAlive);
		response->setConnectionStatus();

/*		if(request->formData()!=nullptr)
			for(const auto &[key, val]: *request->formData())
			{
				std::cout<<"\""<<key<<"\": ";
				for(const auto &e: *val)
					std::cout<<"\""<<e<<"\",";
				std::cout<<std::endl;
			}*/
/*		if(request->multipartFormData()!=nullptr)
			for(const auto &[key, val]: *request->multipartFormData())
			{
				std::cout<<"\""<<key<<"\":\n";
				for(const auto &e: *val)
					std::cout<<"File name: "<<e->fileName<<"; Content type: "<<e->contentType<<"; Content: \n\""<<*e->content<<"\"\n";
			}*/


		std::cout<<"Parsing complete"<<std::endl;
		delete request;
		const bool writeSuccess=co_await writeResponse(socket, response);
		if(!keepAlive||!writeSuccess)
		{
			socket.close();
			co_return;
		}
		waitNextRequest=true;
	}
}

boost::asio::awaitable<void> startListen()
{
	const auto executor=co_await boost::asio::this_coro::executor;
	tcp::acceptor acceptor{executor, {tcp::v4(), 8080}};
	auto *manager=new ConnectionManager(executor);

	for(;;)
	{
		std::cout<<"wait for connection"<<std::endl;
		tcp::socket socket=co_await acceptor.async_accept(boost::asio::use_awaitable);
		std::cout<<"incoming connection"<<std::endl;
//		boost::asio::co_spawn(executor, handleConnection(std::move(socket)), boost::asio::detached);
		boost::asio::co_spawn(executor, [manager, socket=std::move(socket)]() mutable -> boost::asio::awaitable<void>
			{
				try
				{
					//co_await manager->acquire();
					if(!(co_await manager->acquire()))
					{
						socket.close();
						co_return;
					}
					try
					{
						co_await handleConnection(std::move(socket));
					}
					catch(...)
					{
						manager->release();
						throw;
					}
					manager->release();
				}
				catch(const std::runtime_error&)
				{
					socket.close();
				}

			},
			boost::asio::detached);
	}
}

void run()
{
	boost::asio::io_context ioContext;
	std::cout<<"server started"<<std::endl;
	boost::asio::co_spawn(ioContext, startListen, boost::asio::detached);
	ioContext.run();
}

int main()
{
	run();
	return 0;
}
