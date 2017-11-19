/**
 *
 * The thread pool class.
 * You can start any callable objects: functors, global and static function, and class methods
 *
 */

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
#include <atomic>
#include <utility>

class ThreadPool {
public:

	explicit ThreadPool(size_t threadsNumber);

	/**
	* Add a new task to the task queue: functors, functions, class methods
	*/
	template<class Fun, class... Args>
	auto enqueue(Fun&& f, Args&&... args)
		->std::future<typename std::result_of<Fun(Args...)>::type>;
	
	size_t getThreadNumber() const;

	/**
	* Join all worker threads
	*/
	void joinAll();

	~ThreadPool();
private:
	size_t threadsNumber;

	// storage of worker threads
	std::vector<std::thread> workers;

	// the task queue
	std::queue<std::function<void()>> tasks;

	// synchronization
	std::atomic_bool stop;
	std::mutex queue_mutex;
	std::condition_variable condition;
};

// Initialize a specified number of worker threads
inline ThreadPool::ThreadPool(size_t threadsNumber) : threadsNumber(threadsNumber), stop(false) {

	// if the number of threads that system can handle concurrently more than {threadsNumber}, this value will be set
	if (threadsNumber < std::thread::hardware_concurrency()) {
		this->threadsNumber = std::thread::hardware_concurrency();
	}
	for (size_t i = 0; i < this->threadsNumber; ++i)
		workers.emplace_back([this]() {
		for (;;) {
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(queue_mutex);
				condition.wait(lock, [this]() {
					return stop || !tasks.empty(); 
				});
				if (stop && tasks.empty())
					return;
				task = std::move(tasks.front());
				tasks.pop();
			}
			task();
		}
	}
	);
}

/**
 * adds a task to the task queue and returns std::future on result
 */
template<class Fun, class ...Args>
inline auto ThreadPool::enqueue(Fun && f, Args && ...args) 
-> std::future<typename std::result_of<Fun(Args ...)>::type>
{
	using return_type = typename std::result_of<Fun(Args...)>::type;

	auto task = std::make_shared<std::packaged_task<return_type()> >(
		std::bind(std::forward<Fun>(f), std::forward<Args>(args)...)
		);

	std::future<return_type> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		tasks.emplace([task]() { (*task)(); });
	}
	condition.notify_one();
	return res;
}

/** 
 * the destructor joins all threads
 */
inline ThreadPool::~ThreadPool() {
	joinAll();
}

inline size_t ThreadPool::getThreadNumber() const {
	return threadsNumber;
}

inline void ThreadPool::joinAll() {		
	{		
		std::unique_lock<std::mutex> lock(queue_mutex);
		stop = true;
	}
	condition.notify_all();
	for (auto& x : workers) {
		if (x.joinable()) {
			x.join();
		}
	}
}

#endif
