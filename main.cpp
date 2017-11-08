#include <iostream>
#include <vector>
#include <chrono>

#include "ThreadPool.h"

std::mutex m;

int main()
{

    ThreadPool pool(4);
    std::vector< std::future<int> > results;

    for (int i = 0; i < 8; ++i) {
        results.emplace_back(
                pool.enqueue([i] {
                    std::lock_guard<std::mutex> lock(m);
                    std::cout << "hello " << i << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    std::cout << "world " << i << std::endl;
                    return i*i;
                })
        );
    }

    pool.joinAll();

    for (auto && result : results)
        std::cout << result.get() << ' ';
    std::cout << std::endl;

    system("pause");
    return 0;
}