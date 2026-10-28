#ifndef CONNECTION_MANAGER
#define CONNECTION_MANAGER

#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>

#include <deque>

using boost::asio::ip::tcp;
using PermitChannel=boost::asio::experimental::channel<boost::asio::any_io_executor, void(boost::system::error_code, bool)>;

enum class PendingState
{
	Pending,
	Granted,
	Cancelled
};

struct PendingConnection
{
	PermitChannel channel;
	PendingState state;
};

class ConnectionManager
{
	public:
		ConnectionManager(boost::asio::any_io_executor executor): executor_(std::move(executor)){};
		boost::asio::awaitable<bool> acquire();
		void release();
		void stop();
		std::size_t active(){return active_;}
		std::size_t waiting(){return queue_.size();}
		~ConnectionManager();
	private:
		boost::asio::any_io_executor executor_;
		size_t active_=0;
		bool stopping_=false;
		std::deque<PendingConnection*> queue_;
};

#endif
