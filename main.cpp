#include <boost/asio.hpp>
//#include <boost/regex.hpp>
#include <string>
#include <string_view>
#include <iostream>

#include "request.hpp"
#include "response.hpp"

#ifndef HEADERS_SIZE_LIMIT
#define HEADERS_SIZE_LIMIT 1024*16
#endif
#ifndef CONTENT_LENGTH_LIMIT
#define CONTENT_LENGTH_LIMIT 1024*1024
#endif

using boost::asio::ip::tcp;

boost::asio::awaitable<void> handleResponse(tcp::socket socket, Response *response)
{
	std::cout<<" prepare response"<<std::endl;
        boost::asio::streambuf sResponse;
        std::ostream oStream(&sResponse);
	response->setContentLength();
	response->toStream(oStream);
        std::cout<<" response prepared"<<std::endl;
        co_await boost::asio::async_write(socket, sResponse, boost::asio::use_awaitable);
	socket.close();
	delete response;
}

boost::asio::awaitable<void> handleRequest(tcp::socket socket)
{
	boost::asio::streambuf sRequest(HEADERS_SIZE_LIMIT);
	std::cout<<" read request"<<std::endl;
/*
There was read request until \r\n\r\n. But if you have a large request, 
e.g. file post request boost with delimiter will read first 1024 bytes only.
So I changed read logic to:
I Read request headers with ^\r\n regex
II Find Content-Length header and get request size
III If there is Content-Length read until it's value. 
	If there is no then request is short and we have alredy readed it.
*/

	boost::system::error_code ec;
	auto bytesTransferred=co_await boost::asio::async_read_until(socket, sRequest, "\r\n\r\n", boost::asio::redirect_error(boost::asio::use_awaitable, ec));
	auto *response=new Response();
	if(ec)
	{
		//delimiter was not found and current buffer size is buffer size limit
		if(ec==boost::asio::error::not_found&&sRequest.size()==HEADERS_SIZE_LIMIT)
			response->responseCode(431);
		else
			response->responseCode(500);
		const auto executor=co_await boost::asio::this_coro::executor;
		boost::asio::co_spawn(executor, handleResponse(std::move(socket), response), boost::asio::detached);
		co_return;
	}
	std::istream is{&sRequest};

	auto *request=new Request();
	request->parseHeaders(is, bytesTransferred);


//	auto bufs=sRequest.data();
//	std::string strRequest{boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs)+bytesTransferred};

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
			std::cout<<"Failed to stoi content-length header, exception:\n\t\""<<e.what()<<"\"\n";
		}
		if(contentLength>CONTENT_LENGTH_LIMIT)
		{
			response->responseCode(413);
			const auto executor=co_await boost::asio::this_coro::executor;
			boost::asio::co_spawn(executor, handleResponse(std::move(socket), response), boost::asio::detached);
			co_return;

		}

		auto remainingStreamBytes=sRequest.size();
		std::string body(contentLength, '\0');
		if(remainingStreamBytes>=contentLength)
		{
			is.read(&body[0], contentLength);
			request->body(body);
		}
		else
		{
			is.read(&body[0], remainingStreamBytes);
			//TODO: async read remaining body, handle errors, etc
		}

	}

//	if(strRequest.find("Content-Length")!=std::string::npos)
//	{
//		auto contentLengthStr=strRequest.substr(strRequest.find("Content-Length"));
//		contentLengthStr=contentLengthStr.substr(contentLengthStr.find(' ')+1, contentLengthStr.find("\r")-16);
//		contentLength=std::stoi(contentLengthStr);
//	}

//boost's (async_)read_until reads even after delimiter, but returns size before delimiter. 
//	Even we just wanted to read the headers it also reads part of body.
//	So we have to save data that still in the stream first, 
//	consume it from the stream and then read with loop the rest.
/*	sRequest.consume(bytesTransferred);
	auto alreadyTransferredBody=sRequest.size();
	bufs=sRequest.data();
//	strRequest+=std::string{boost::asio::buffer_cast<const char*>(sRequest.data()), sRequest.size()};
	strRequest+=std::string{boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs)+sRequest.size()};
	sRequest.consume(sRequest.size());

	if(strRequest.size()!=bytesTransferred+contentLength)
	{
		contentLength-=alreadyTransferredBody;
		bytesTransferred=0;
		while(bytesTransferred<contentLength)
		{
//I choosed 1024 bytes per iteration because it is standard boost boofer size.
//It can be changed to less or more by awaitBytes val and setting boofer size.
			auto awaitBytes=1024;
			if((contentLength-bytesTransferred)<awaitBytes)
				awaitBytes=contentLength-bytesTransferred;
			auto iterationBytes=co_await boost::asio::async_read(socket, sRequest, boost::asio::transfer_exactly(awaitBytes), boost::asio::use_awaitable);
			bytesTransferred+=iterationBytes;
//			strRequest+=std::string{boost::asio::buffer_cast<const char*>(sRequest.data()), iterationBytes};
			bufs=sRequest.data();
			strRequest+=std::string{boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs)+iterationBytes};
			sRequest.consume(iterationBytes);
		}
	}

	std::cout<<"\""<<strRequest<<"\""<<std::endl;

//	Request *request=new Request(std::move(strRequest));
//	request->parse();*/
	std::cout<<"Parsing complete"<<std::endl;
	delete request;
//	Response *response=new Response();
	const auto executor=co_await boost::asio::this_coro::executor;
	boost::asio::co_spawn(executor, handleResponse(std::move(socket), response), boost::asio::detached);
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
		boost::asio::co_spawn(executor, handleRequest(std::move(socket)), boost::asio::detached);
		std::cout<<"after set executor"<<std::endl;
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
