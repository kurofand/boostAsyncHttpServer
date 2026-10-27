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

	auto *waiter=new PendingConnection{PermitChannel(executor_, 1), PendingState::Pending};
	queue_.push_back(waiter);

	(void)co_await waiter->channel.async_receive(boost::asio::use_awaitable);

	delete waiter;
}

void ConnectionManager::release()
{
	if(active_==0)
		return;

	while(!queue_.empty())
	{
		auto *waiter=queue_.front();
		queue_.pop_front();

		if(waiter->state!=PendingState::Pending)
			continue;

		waiter->state=PendingState::Granted;

		if(waiter->channel.try_send(boost::system::error_code{}, true))
			return;
		//TODO:
		//handle try_send error
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
