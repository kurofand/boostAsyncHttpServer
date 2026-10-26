#ifndef CONNECTION_MANAGER
#define CONNECTION_MANAGER

#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>

#include <deque>

using boost::asio::ip::tcp;

class ConnectionManager
{
	public:
		ConnectionManager(boost::asio::any_io_executor executor): executor_(std::move(executor)){};
		boost::asio::awaitable<void> acquire();
		void release();
		std::size_t active(){return active_;}
		std::size_t waiting(){return queue_.size();}
		~ConnectionManager();
	private:
		using PermitChannel=boost::asio::experimental::channel<boost::asio::any_io_executor, void(boost::system::error_code, bool)>;
		boost::asio::any_io_executor executor_;
		size_t active_=0;
		std::deque<PermitChannel*> queue_;
};

#endif
