#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool 
{
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> jobs;
    std::mutex queue_mutex;
    std::condition_variable cv;
    
    int active_jobs=0;
    std::condition_variable all_done; 


    bool stop = false;
    void worker_loop();


public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();
    void submit(std::function<void()> job);
    void wait_all();
};