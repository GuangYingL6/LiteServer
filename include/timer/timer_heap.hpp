#ifndef __TIMER_HEAP_HPP__
#define __TIMER_HEAP_HPP__

#include <queue>
#include <unordered_map>
#include <chrono>
#include <mutex>

#include <sys/time.h>

using timepoint = std::chrono::steady_clock::time_point;

const int TIMEOUTSEC = 30;

struct timedata
{
    timedata() {}
    timedata(const int _fd, const timepoint &_time) : fd(_fd), time(_time) {}
    ~timedata() {}
    int fd{};
    timepoint time{};

    bool operator<(const timedata& td) const {
        return this->time < td.time;
    }
};

struct retimedata {
    retimedata() : is_timeout(true), 
                newtimer(std::chrono::steady_clock::now() + std::chrono::seconds(TIMEOUTSEC)) {}
    bool is_timeout{true};
    std::mutex mtx;
    timepoint newtimer;

private:
    void update(int64_t sec = TIMEOUTSEC)
    {
        if (!is_timeout)
            return;
        is_timeout = false;
        auto outt = std::chrono::seconds(sec);
        newtimer = std::chrono::steady_clock::now() + outt;
    }

    static timepoint getnow() {
        return std::chrono::steady_clock::now();
    }

public:
    timepoint gettimer() {
        std::unique_lock<std::mutex> lock(mtx);
        return newtimer;
    }
    bool check_update()
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (getnow() < newtimer) {
            update();
            return true;
        }
        return false;
    };
};

class TimerHeap
{
    static std::mutex ptrmtx;
    static TimerHeap *ptr;
    TimerHeap() {
        startimer();
    }

public:
    std::mutex mtx{};
    std::priority_queue<timedata> heap{};

    std::mutex vecmtx{};
    std::vector<timedata> vec{};

    struct itimerval oncetimer;
    static TimerHeap* make_timerheap() {
        if (ptr == nullptr) {
            std::unique_lock<std::mutex> lock(ptrmtx);
            if (ptr == nullptr) {
                ptr = new TimerHeap;
            }
        }
        return ptr;
    }

    void add(const timedata &data) {
        std::unique_lock<std::mutex> lock(mtx);
        heap.push(data);
    }

    std::vector<timedata> getoutvec() {
        std::vector<timedata> v{};
        std::unique_lock<std::mutex> lock(vecmtx);
        std::swap(v, vec);
        return v;
    }

    void tick()
    {
        if (heap.empty())
            return;
        std::vector<timedata> localvec{};
        auto now = std::chrono::steady_clock::now();
        {
            std::unique_lock<std::mutex> lock(mtx);
            while (heap.top().time < now) {
                localvec.push_back(std::move(heap.top()));
                heap.pop();
            }
            if (!heap.empty()) {
                auto sec = heap.top().time - now;
                std::cout << sec.count() << std::endl;
                startimer(sec.count()); // 重启定时器
            }
        }
        {
            std::unique_lock<std::mutex> lock(vecmtx);
            vec = std::move(localvec);
        }
    }

    void startimer(time_t sec = TIMEOUTSEC, suseconds_t usec = 0) {
        struct itimerval check;
        getitimer(ITIMER_REAL, &check);
        if (check.it_value.tv_sec != 0 || check.it_value.tv_usec != 0) {
            return;
        }
        oncetimer.it_value.tv_sec = sec;
        oncetimer.it_value.tv_usec = usec;
        oncetimer.it_interval.tv_sec = 0;
        oncetimer.it_interval.tv_usec = 0;

        setitimer(ITIMER_REAL, &oncetimer, NULL); // 启动定时器
    }
    void stoptimer() {
        struct itimerval disable = {{0, 0}, {0, 0}};
        setitimer(ITIMER_REAL, &disable, NULL);
    }
};

std::mutex TimerHeap::ptrmtx{};
TimerHeap *TimerHeap::ptr{nullptr};

#endif