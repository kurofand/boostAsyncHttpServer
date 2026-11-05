#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <string>
#include <string_view>
#include <iostream>
#include <csignal>

#include "request.hpp"
#include "response.hpp"
#include "connectionManager.hpp"

#include "Parsers/multipartParser.hpp"
#include "Parsers/urlencodedParser.hpp"

#include "common.hpp"
#include "config.hpp"

#include "Log/log.hpp"

using boost::asio::ip::tcp;

boost::asio::awaitable<bool> writeResponse(tcp::socket &socket, Response *response)
{
	Log(LogLevel::Debug)<<"Preparing response stream";
	boost::asio::streambuf sResponse;
	std::ostream oStream(&sResponse);
	response->setContentLength();
	response->toStream(oStream);
	Log(LogLevel::Debug)<<"Response stream prepared";
	bool res=true;
	boost::system::error_code ec;
	co_await boost::asio::async_write(socket, sResponse, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
	if(ec)
	{
		Log(LogLevel::Error)<<"Error on writing response: "<<ec.message();
		res=false;
	}
	Log(LogLevel::Debug)<<"Response sent";
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


	Parser *parser=nullptr;
	switch(request->contentType())
	{
		case ContentType::APPLICATION_X_WWW_FORM_URLENCODED:
			parser=new UrlencodedParser();
			break;
		case ContentType::MULTIPART_FORM_DATA:
		{
			std::string boundary=request->multipartBoundary();
			if(boundary.empty())
				co_return ReadResult::BAD_REQUEST;
			parser=new MultipartParser(boundary, contentLength);
			break;
		}
		default:
			co_return ReadResult::UNSUPPORTED_MEDIA_TYPE;
	};
	if(available>=contentLength)
	{
		is.read(&body[0], contentLength);
		std::string_view chunk(body.c_str(), body.size());
		auto parseRes=parser->parseChunk(chunk);
		delete parser;
		co_return parseRes;
	}
	else
	{
		is.read(&body[0], available);
		std::string_view chunk(body.c_str(), available);
		auto parseRes=parser->parseChunk(chunk);
		if(parseRes!=ReadResult::OK)
		{
			delete parser;
			co_return parseRes;
		}

		size_t remaining=contentLength-available;
		char buf[READ_CHUNK_SIZE];
		while(remaining>0)
		{
			timedout=false;
			bool chunkRead=false;
			const auto readSize=std::min(remaining, static_cast<size_t>(READ_CHUNK_SIZE));
			timer.expires_after(std::chrono::seconds(BODY_IDLE_TIMEOUT));
			timer.async_wait([&socket, &timedout, &chunkRead](const boost::system::error_code &ec)
				{
					if(ec||chunkRead)
						return;
					timedout=true;
					socket.cancel();
				});
			const auto bytes=co_await socket.async_read_some(boost::asio::buffer(buf, readSize), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
			chunkRead=true;
			timer.cancel();
			if(ec)
			{
				delete parser;
				if(ec==boost::asio::error::operation_aborted&&timedout)
					co_return ReadResult::TIMEOUT;
				else
					co_return ReadResult::NETWORK_ERROR;
			}

			chunk=std::string_view(buf, bytes);
			parseRes=parser->parseChunk(chunk);
			if(parseRes!=ReadResult::OK)
			{
				delete parser;
				co_return parseRes;
			}
			remaining-=bytes;
		}
		request->parsedBody(parser->result());
	}
	delete parser;
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
					Log(LogLevel::Debug)<<"Connection timeout";
					timedout=true;
					socket.close();
				});
		}
		Log(LogLevel::Debug)<<"Reading request";

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

		const auto requestProtoVer=request->protocolVersion();
		if(requestProtoVer.maj!=0)
			response->protocolVersion(requestProtoVer);

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

		if(request->parsedBody()!=nullptr)
		{
			std::cout<<"aaaaqa\n";
			auto *multipartFormData=request->parsedBody()->multipartFormData;
			if(multipartFormData!=nullptr)
				for(const auto &[key, val]: *multipartFormData)
				{
					std::cout<<"\""<<key<<"\":\n";
					for(const auto &e: *val)
						std::cout<<"File name: "<<e->fileName<<"; Content type: "<<e->contentType<<"; Content: \n\""<<*e->content<<"\"\n";
				}
			auto *formData=request->parsedBody()->formData;
			if(formData!=nullptr)
				for(const auto &[key, val]: *formData)
				{
					std::cout<<"\""<<key<<"\": ";
					for(const auto &e: val)
						std::cout<<"\""<<e<<"\",";
					std::cout<<std::endl;
				}
		}


		Log(LogLevel::Debug)<<"Request parsing complete";
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
	boost::asio::signal_set signals(executor, SIGINT, SIGTERM);

	signals.async_wait(
		[&acceptor, manager](const boost::system::error_code &ec, int signal)
		{
			if(ec)
				return;
			//wait for signal and stop the server
			Log(LogLevel::Info)<<"Server shutdown initiated";
			acceptor.close();
			manager->stop();
		});


	for(;;)
	{
		Log(LogLevel::Debug)<<"Waiting for a new connection";
		boost::system::error_code ec;
		tcp::socket socket=co_await acceptor.async_accept(boost::asio::redirect_error(boost::asio::use_awaitable, ec));

		if(ec==boost::asio::error::operation_aborted)
			break;

		Log(LogLevel::Debug)<<"Incoming connection";
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
	Log(LogLevel::Info)<<"server started";
	boost::asio::co_spawn(ioContext, startListen, boost::asio::detached);
	ioContext.run();
}

int main()
{
	run();
	return 0;
}
