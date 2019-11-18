#pragma once

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <chrono>

class Barrier
{
public:
	Barrier(size_t number);
    
	void wait();
    bool timedWait(std::chrono::microseconds timeout);
	void reset();
    
private:
	bool step();

	boost::interprocess::interprocess_condition condition;
	boost::interprocess::interprocess_mutex     mutex;
	size_t number;
	size_t counter;
};
