#ifndef TIMER_HEAP_H
#define TIMER_HEAP_H

#include <memory>
#include <queue>
#include <chrono>
#include <functional>
#include <unordered_map>

#include <muduo/net/TcpConnection.h>
#include <muduo/base/Timestamp.h>
#include <muduo/base/Logging.h>


class TimerHeap {
public:
    using TcpConnectionPtr = muduo::net::TcpConnectionPtr;
    using TimeoutCallback = std::function<void()>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct TimerNode {
        int64_t id;  // 使用连接对象的地址作为唯一标识
        TimePoint expires;
        TimeoutCallback cb;
        
        bool operator<(const TimerNode& t) const {
            return expires > t.expires;  // 小根堆
        }
    };

    void add(const TcpConnectionPtr& conn, int timeout_ms, const TimeoutCallback& cb);
    void del(const TcpConnectionPtr& conn);
    void tick();
    int getNextTickDelay();  // 获取下次需要tick的时间间隔（毫秒）

private:
    std::priority_queue<TimerNode> heap_;
    std::unordered_map<int64_t, TimerNode> ref_;  // 快速查找
};


#endif