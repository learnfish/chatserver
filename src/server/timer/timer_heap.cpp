#include "timer_heap.hpp"
//还没用到
void TimerHeap::add(const TcpConnectionPtr& conn, int timeout_ms, const TimeoutCallback& cb) {
    int64_t id = reinterpret_cast<int64_t>(conn.get());  // 使用连接对象地址作为ID
    auto now = Clock::now();
    auto node = TimerNode{
        id,
        now + std::chrono::milliseconds(timeout_ms),
        cb
    };
    
    heap_.push(node);
    ref_[id] = node;
}

void TimerHeap::del(const TcpConnectionPtr& conn) {
    int64_t id = reinterpret_cast<int64_t>(conn.get());
    ref_.erase(id);
    // 注意：堆中节点不会立即删除，会在tick时跳过已删除的节点
}

void TimerHeap::tick() {
    auto now = Clock::now();
    while (!heap_.empty()) {
        const auto& node = heap_.top();
        if (node.expires > now) break;  // 未超时
        
        // 检查节点是否已被删除
        if (ref_.find(node.id) != ref_.end() && ref_[node.id].expires == node.expires) {
            node.cb();  // 执行超时回调
            ref_.erase(node.id);
        }
        heap_.pop();
    }
}

int TimerHeap::getNextTickDelay() {
    tick();  // 先处理已超时的节点
    
    if (heap_.empty()) {
        return 10000;  // 默认10秒后再次检查
    }
    
    auto now = Clock::now();
    auto next = heap_.top().expires;
    return std::chrono::duration_cast<std::chrono::milliseconds>(next - now).count();
}