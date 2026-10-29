#include "connectionManager.hpp"
#include "config.hpp"

boost::asio::awaitable<bool> ConnectionManager::acquire()
{
	if(stopping_)
		co_return false;

	if(active_<MAX_CLIENTS&&queue_.empty())
	{
		active_++;
		co_return true;
	}

	if(queue_.size()>=MAX_PENDING_CLIENTS)
		throw std::runtime_error("Queue is full");

	auto *waiter=new PendingConnection{PermitChannel(executor_, 1), PendingState::Pending};
	queue_.push_back(waiter);

	boost::system::error_code ec;
	bool granted;
	granted=co_await waiter->channel.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
	if(ec)
	{
		const auto pos=std::find(queue_.begin(), queue_.end(), waiter);
		if(pos!=queue_.end())
			queue_.erase(pos);
		granted=false;
	}

	delete waiter;
	co_return granted;
}

void ConnectionManager::release()
{
	if(active_==0)
		return;

	if(stopping_)
	{
		active_--;
		return;
	}

	while(!queue_.empty())
	{
		auto *waiter=queue_.front();
		queue_.pop_front();

		if(waiter->state!=PendingState::Pending)
			continue;


		if(waiter->channel.try_send(boost::system::error_code{}, true))
		{
			waiter->state=PendingState::Granted;
			return;
		}
		else
		{
			waiter->state=PendingState::Cancelled;
			waiter->channel.cancel();
		}
	}
	active_--;
}

void ConnectionManager::stop()
{
	if(stopping_)
		return;

	stopping_=true;
	for(auto *waiter: queue_)
	{
		if(waiter->state!=PendingState::Pending)
			continue;

		waiter->state=PendingState::Cancelled;
		if(!waiter->channel.try_send(boost::system::error_code{}, false))
			waiter->channel.cancel();
	}
	queue_.clear();
}

ConnectionManager::~ConnectionManager()
{
	stop();
}
