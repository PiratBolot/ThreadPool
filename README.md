# ThreadPool
Simple Thread Pool in C++11 with futures

## Usage:

We have several functions and class

```c++
std::mutex m;

struct SS {
public:
	int foo(int i) {
		{
			std::lock_guard<std::mutex> lock(m);
			std::cout << "Hello foo #" << i << std::endl;
		}
		return i;
	}

	static int staticFoo(int i) {
		{
			std::lock_guard<std::mutex> lock(m);
			std::cout << "Hello staticFoo #" << i << std::endl;
		}
		return i;
	}
};

auto lambdaFoo = [](int i) -> int {
	{
		std::lock_guard<std::mutex> lock(m);
		std::cout << "Hello lambdaFoo #" << i << std::endl;
	}
	return i;
};

int globalFoo(int i) {
	{
		std::lock_guard<std::mutex> lock(m);
		std::cout << "Hello globalFoo #" << i << std::endl;
	}
	return i;
}
```
Then we can add them in ThreadPool as tasks

```c++
ThreadPool pool(10);
	std::vector< std::future<int> > results;
	SS a;
	int count = 60;
	
	// Add class method
	for (int i = 0; i < count; ++i) {
		results.emplace_back(
			pool.enqueue(&SS::foo, a, i)
		);
	}

	// Add class static method
	for (int i = 0; i < count; ++i) {
		results.emplace_back(
			pool.enqueue(&SS::staticFoo, i)
		);
	}
	
	// Add finished lambda
	for (int i = 0; i < count; ++i) {
		results.emplace_back(
			pool.enqueue(lambdaFoo, i)
		);
	}

	// Add global function
	for (int i = 0; i < count; ++i) {
		results.emplace_back(
			pool.enqueue(globalFoo, i)
		);
	}

	// Define and add lambda
	for (int i = 0; i < count; ++i) {
		results.emplace_back(
			pool.enqueue([i]() -> int {
				{
					std::lock_guard<std::mutex> lock(m);
					std::cout << "Hello localLambda #" << i << std::endl;
				}
				return i;
		})
		);
	}

	//std::this_thread::sleep_for(std::chrono::seconds(10));

	pool.joinAll();

	for (auto && result : results)
		std::cout << result.get() << ' ';
	std::cout << std::endl;

```
