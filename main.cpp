#include <boost/asio.hpp>
#include <string>
#include <string_view>
#include <iostream>
#include "request.cpp"

using boost::asio::ip::tcp;

boost::asio::awaitable<void> handleResponse(tcp::socket socket)
{
	std::cout<<" prepare response"<<std::endl;
        boost::asio::streambuf sResponse;
        std::ostream oStream(&sResponse);
        oStream<<"HTTP/1.0 200 OK\r\n"
                <<"Content-Type: text/html; charset=UTF-8;"
                <<"Content-Length: "<<2<<"\r\n\r\n"
                <<"\r\n";
        std::cout<<" response prepared"<<std::endl;
        co_await boost::asio::async_write(socket, sResponse, boost::asio::use_awaitable);
	socket.close();
}

boost::asio::awaitable<void> handleRequest(tcp::socket socket)
{
	boost::asio::streambuf sRequest;
	std::cout<<" read request"<<std::endl;
	co_await boost::asio::async_read_until(socket, sRequest, "\r\n\r\n", boost::asio::use_awaitable);
	std::string strRequest{boost::asio::buffer_cast<const char*>(sRequest.data()), sRequest.size()};
	Request *request=new Request(std::move(strRequest));
	request->parse();
	delete request;
	const auto executor=co_await boost::asio::this_coro::executor;
	boost::asio::co_spawn(executor, handleResponse(std::move(socket)), boost::asio::detached);
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
