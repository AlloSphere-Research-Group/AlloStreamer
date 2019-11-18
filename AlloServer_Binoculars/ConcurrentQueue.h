#include <mutex>
#include <condition_variable>
#include <thread>
#include <stdio.h>
#include <queue>

template<typename Data>
class ConcurrentQueue
{
private:
    std::queue<Data> the_queue;
    mutable std::mutex the_mutex;
    std::condition_variable the_condition_variable;
    FILE* queueFile;
public:
  ConcurrentQueue()
  {
	  queueFile = fopen(ROOT_DIR "/Logs/queue.log", "w");
  }
    void push(Data const& data)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        //fprintf(queueFile, "1Pushing, queue size: %i\n", the_queue.size());
        //fflush(queueFile);
        the_queue.push(data);
        lock.unlock();
        the_condition_variable.notify_one();
    }
    
    bool empty() const
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        return the_queue.empty();
    }
    
    bool tryPop(Data& popped_value)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        if(the_queue.empty())
        {
            return false;
        }
        
        popped_value=the_queue.front();
        the_queue.pop();
        return true;
    }
    
    void waitAndPop(Data& popped_value)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        
        while(the_queue.empty())
        {
            the_condition_variable.wait(lock);
        }
      
        popped_value=the_queue.front();
        //fprintf(queueFile, "2Popping, queue size: %i\n", the_queue.size());
        //fflush(queueFile);
        the_queue.pop();
    }
    int get_size()
    {
        return the_queue.size();
    }
    
};
