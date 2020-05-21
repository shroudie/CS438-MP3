#ifndef _THREAD_QUEUE_H
#define _THREAD_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

namespace sf {

template<typename _tp>
class queue {
public:
    queue() { }

    void push(const _tp& x) {
        std::unique_lock<std::mutex> lk(m);
        q.push(x);
        lk.unlock();
        cv.notify_one();
    }

    void pull(_tp& x) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [this]{ return !q.empty(); });
        x = q.front();
        q.pop();
    }

    ssize_t size() const { return q.size(); }

private:
    std::queue<_tp> q;
    std::mutex m;
    std::condition_variable cv;
};

}

#endif