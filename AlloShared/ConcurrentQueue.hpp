#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename Data>
class ConcurrentQueue
{
private:
	std::queue<Data> queue;
	mutable std::mutex mutex;
    std::condition_variable conditionVariable;
	bool isClosed_;

public:
	ConcurrentQueue() : isClosed_(false)
	{

	}

	void push(Data const& data)
	{
        std::unique_lock<std::mutex> lock(mutex);
		queue.push(data);
		lock.unlock();
		conditionVariable.notify_one();
	}

	bool empty() const
	{
        std::unique_lock<std::mutex> lock(mutex);
		return queue.empty();
	}

	bool tryPop(Data& popped_value)
	{
        std::unique_lock<std::mutex> lock(mutex);
		if (isClosed_ || queue.empty())
		{
			return false;
		}

		popped_value = queue.front();
		queue.pop();
		return true;
	}

	bool waitAndPop(Data& popped_value)
	{
        std::unique_lock<std::mutex> lock(mutex);
		while (!isClosed_ && queue.empty())
		{
			conditionVariable.wait(lock);
		}

		if (isClosed_)
		{
			return false;
		}

		popped_value = queue.front();
		queue.pop();
		return true;
	}

	void close()
	{
        std::unique_lock<std::mutex> lock(mutex);
        std::queue<Data> emptyQueue;
        queue.swap(emptyQueue); // clears queue
		isClosed_ = true;
		conditionVariable.notify_all();
	}

	size_t size()
	{
        std::unique_lock<std::mutex> lock(mutex);
		return queue.size();
	}

	bool isClosed()
	{
		return isClosed_;
	}
};
