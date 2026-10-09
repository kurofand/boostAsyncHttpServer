#include <boost/asio.hpp>
#include <string>
#include <string_view>
#include <iostream>

#include "request.hpp"
#include "response.hpp"

#include "common.hpp"

#ifndef HEADERS_SIZE_LIMIT
#define HEADERS_SIZE_LIMIT 1024*16
#endif
#ifndef CONTENT_LENGTH_LIMIT
#define CONTENT_LENGTH_LIMIT 1024*1024
#endif
#ifndef KEEP_ALIVE_TIMEOUT
#define KEEP_ALIVE_TIMEOUT 10
#endif

using boost::asio::ip::tcp;

boost::asio::awaitable<void> writeResponse(tcp::socket &socket, Response *response)
{
	std::cout<<"prepare response"<<std::endl;
	boost::asio::streambuf sResponse;
	std::ostream oStream(&sResponse);
	response->setContentLength();
	response->toStream(oStream);
	std::cout<<"response prepared"<<std::endl;
	co_await boost::asio::async_write(socket, sResponse, boost::asio::use_awaitable);
	std::cout<<"response sent"<<std::endl;
	delete response;
}

boost::asio::awaitable<ReadResult> readRequest(tcp::socket &socket, boost::asio::streambuf &buffer, Request *request)
{
	boost::system::error_code ec;
	const auto headerLength=co_await boost::asio::async_read_until(socket, buffer, "\r\n\r\n", boost::asio::redirect_error(boost::asio::use_awaitable, ec));
	if(ec)
	{
		if(ec==boost::asio::error::not_found&&buffer.size()==HEADERS_SIZE_LIMIT)
			co_return ReadResult::HEADERS_TOO_LARGE;
		else
			co_return ReadResult::NETWORK_ERROR;
	}

	std::istream is{&buffer};
	if(!request->parseHeaders(is, headerLength))
		co_return ReadResult::BAD_REQUEST;
	unsigned int contentLength=0;
	if(request->headers()!=nullptr&&request->headers()->contains("content-length"))
	{
		const auto tmp=request->headers()->at("content-length");
		try
		{
			contentLength=std::stoi(tmp);
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
		const size_t remaining=contentLength-available;
		co_await boost::asio::async_read(socket, boost::asio::buffer(body.data()+available, remaining), boost::asio::transfer_exactly(remaining), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
		if(ec)
			co_return ReadResult::NETWORK_ERROR;
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
			{
				response=new Response();
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
		co_await writeResponse(socket, response);
		if(!keepAlive)
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
	for(;;)
	{
		std::cout<<"wait for connection"<<std::endl;
		tcp::socket socket=co_await acceptor.async_accept(boost::asio::use_awaitable);
		std::cout<<"incoming connection"<<std::endl;
		boost::asio::co_spawn(executor, handleConnection(std::move(socket)), boost::asio::detached);
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
