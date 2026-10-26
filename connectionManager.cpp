#include "connectionManager.hpp"
#include "config.hpp"

boost::asio::awaitable<void> ConnectionManager::acquire()
{
	if(active_<MAX_CLIENTS&&queue_.empty())
	{
		active_++;
		co_return;
	}

	if(queue_.size()>=MAX_PENDING_CLIENTS)
		throw std::runtime_error("Queue is full");

	auto *waiter=new PermitChannel(executor_, 1);
	queue_.push_back(waiter);

	(void)co_await waiter->async_receive(boost::asio::use_awaitable);
}

void ConnectionManager::release()
{
	if(active_==0)
		return;

	while(!queue_.empty())
	{
		auto *waiter=queue_.front();
		queue_.pop_front();

		if(waiter->try_send(boost::system::error_code{}, true))
		{
			delete waiter;
			return;
		}
	}
	active_--;
}

ConnectionManager::~ConnectionManager()
{
	while(!queue_.empty())
	{
		delete queue_.front();
		queue_.pop_front();
	}
}
