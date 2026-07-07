#include "thread_pool.h"//includes the header file where class is declared


void ThreadPool::worker_loop()
{
    while(true)
    {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [this]{ return !jobs.empty() || stop; });
            if(stop && jobs.empty()){
                return; // no more work coming, shut down
            }
            job = jobs.front();
            jobs.pop();
        } // lock released here, since `lock` goes out of scope — happens every iteration, not just at shutdown
        job();
        // Job might create children
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            active_jobs-=1;
            if(active_jobs==0)
            {
                all_done.notify_all();
            }
        }
    }
}


void ThreadPool::wait_all()
{
    std::unique_lock<std::mutex> lock(queue_mutex);
    all_done.wait(lock,[this]{return active_jobs==0;});

}



ThreadPool::ThreadPool(size_t num_threads)//constructs outside class
{
    for (size_t i = 0; i < num_threads; ++i)//loops through the threads to assign worker loop
    {
        workers.emplace_back(&ThreadPool::worker_loop, this);//acutally assign each thread its own worker loop
    }
}


void ThreadPool::submit(std::function<void()> job)
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);//declares and locks mutex
        jobs.push(job); //push job into the queue
        active_jobs+=1;
    }
    cv.notify_one(); //notify the threads about a job being present
}


ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);//Locks mutex
        stop=true;//asks to stop
    }
    cv.notify_all();
    for (std::thread& worker : workers)//for each worker thread
        {
            worker.join();//is asked to finish their tasks and go to sleep
        }
    
}