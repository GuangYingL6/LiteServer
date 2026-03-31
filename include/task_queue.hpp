#ifndef __TASL_QUEUE_HPP__
#define __TASL_QUEUE_HPP__

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

class task_queue
{
    std::queue<std::function<void()>> que;
    std::mutex mtx;
    std::condition_variable cond;

    int THREAD_NUM = 1;
    std::vector<std::thread> threads;
    bool stop = false;

public:
    task_queue()
    {
        for (int i = 0; i < THREAD_NUM; ++i)
        {
            threads.emplace_back([this]()
                                 {
                while (true) {
                    std::unique_lock<std::mutex> lock(mtx);
                    cond.wait(lock, [this]()
                              { return !que.empty() || stop; });
                    if (stop && que.empty()) {
                        return;
                    }
                    std::function<void()> func(std::move(que.front()));
                    que.pop();
                    lock.unlock();

                    func();
                } });
        }
    }
    ~task_queue()
    {
        {
            std::unique_lock<std::mutex> lock(mtx);
            stop = true;
        }

        cond.notify_all();
        for (auto &t : threads)
        {
            t.join();
        }
    }

    template <typename F, typename... Args>
    void push(F &&fun, Args &&...args)
    {
        std::function<void()> func;
        if constexpr (!sizeof...(args))
        {
            func = std::bind(std::forward<F>(fun), std::forward<Args>(args)...);
        }
        else
        {
            func = std::move(fun);
        }

        {
            std::unique_lock<std::mutex> lock(mtx);
            que.push(func);
        }
        cond.notify_one();
    }

    std::function<void()> wait_task()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cond.wait(lock, [this]()
                  { return !que.empty(); });
        std::function<void()> func = std::move(que.front());
        que.pop();
        return func;
    }
};

#endif