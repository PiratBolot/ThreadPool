#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t);

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
    ->std::future<typename std::result_of<F(Args...)>::type>;

    void joinAll();

    void waitAll();

    ~ThreadPool();
private:
    // storage of worker threads
    std::vector<std::thread> workers;

    // the task queue
    std::queue<std::function<void()>> tasks;

    // synchronization
    std::atomic_bool stop;
    std::atomic_bool bailout;
    std::mutex queue_mutex;
    std::mutex wait_mutex;
    std::condition_variable condition;
    std::condition_variable wait_var;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads) : bailout(false), stop(false)
{
    // if the number of threads less than the number that system can hadle concurrently, this value will be set
    if (threads < std::thread::hardware_concurrency()) {
        threads = std::thread::hardware_concurrency();
    }
    for (size_t i = 0; i < threads; ++i)
        workers.emplace_back([this]() {
                                 for (;;) {
                                     std::function<void()> task;
                                     {
                                         std::unique_lock<std::mutex> lock(this->queue_mutex);
                                         this->condition.wait(lock,
                                                              [this]() { return this->stop || !this->tasks.empty(); });
                                         if (this->stop && this->tasks.empty())
                                             return;
                                         task = std::move(this->tasks.front());
                                         this->tasks.pop();
                                     }
                                     wait_var.notify_one();
                                     task();
                                 }
                             }
        );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if (stop)
            throw std::runtime_error("Enqueue function is started after stopping threads");

        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    if (!bailout) {
        joinAll();
    }
}

inline void ThreadPool::joinAll() {
    if (!bailout) {
        stop = true;
        ThreadPool::waitAll();

        // note that we're done, and wake up any thread that's
        // waiting for a new job
        bailout = true;
        condition.notify_one();

        for (auto &x : workers) {
            if (x.joinable()) {
                x.join();
            }
        }
        bailout = true;
    }
}

inline void ThreadPool::waitAll() {
    if (!this->tasks.empty()) {
        std::unique_lock<std::mutex> lk(wait_mutex);
        wait_var.wait(lk, [this] { return this->tasks.empty(); });
    }
}

#endif
