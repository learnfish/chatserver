#include "stresstest.hpp"
#include <sys/socket.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

StressTester::StressTester(const string& ip, int port, int userId, const string& password)
    : serverIp_(ip), serverPort_(port), userId_(userId), password_(password) {
    sem_init(&loginSem_, 0, 0);
    connectToServer();
}

StressTester::~StressTester() {
    if (isRunning_) {
        isRunning_ = false;
        if (recvThread_.joinable()) recvThread_.join();
    }
    if (clientfd_ > 0) close(clientfd_);
    sem_destroy(&loginSem_);
}

void StressTester::connectToServer() {
    clientfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd_ == -1) {
        cerr << "StressTester: socket create error" << endl;
        return;
    }

    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(serverPort_);
    server.sin_addr.s_addr = inet_addr(serverIp_.c_str());

    if (connect(clientfd_, (sockaddr*)&server, sizeof(sockaddr_in)) == -1) {
        cerr << "StressTester: connect server error" << endl;
        close(clientfd_);
        clientfd_ = -1;
        return;
    }
    
    // 启动接收线程
    isRunning_ = true;
    recvThread_ = thread(&StressTester::receiveThread, this);
}

bool StressTester::login() {
    if (clientfd_ <= 0) return false;
    
    json js;
    js["msgid"] = LOGIN_MSG;
    js["id"] = userId_;
    js["password"] = password_;
    string request = js.dump();
    
    int len = send(clientfd_, request.c_str(), request.size() + 1, 0);
    if (len <= 0) {
        cerr << "StressTester: send login failed for user " << userId_ << endl;
        return false;
    }
    
    // 等待登录响应
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5; // 5秒超时
    
    if (sem_timedwait(&loginSem_, &ts) == -1) {
        cerr << "StressTester: login timeout for user " << userId_ << endl;
        return false;
    }
    
    return isLoggedIn_;
}

void StressTester::logout() {
    if (!isLoggedIn_ || clientfd_ <= 0) return;
    
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = userId_;
    string buffer = js.dump();
    
    send(clientfd_, buffer.c_str(), buffer.size() + 1, 0);
    isLoggedIn_ = false;
}

void StressTester::sendMessage(int targetId, const string& msg) {
    if (!isLoggedIn_ || clientfd_ <= 0) return;
    
    auto start = chrono::steady_clock::now();
    
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = userId_;
    js["toid"] = targetId;
    js["msg"] = msg;
    js["time"] = ""; // 时间戳在服务端添加
    
    string buffer = js.dump();
    int len = send(clientfd_, buffer.c_str(), buffer.size() + 1, 0);
    
    if (len > 0) {
        messagesSent_++;
    }
}

StressTester::TestResult StressTester::runTest(int messageCount, int intervalMs) {
    TestResult result;
    result.userId = userId_;
    
    if (!login()) {
        result.success = false;
        return result;
    }
    
    for (int i = 0; i < messageCount; i++) {
        int targetId = (userId_ + 1) % 1000; // 简单的目标用户选择策略
        
        stringstream msg;
        msg << "压力测试消息 #" << (i+1) << " from " << userId_;
        sendMessage(targetId, msg.str());
        
        this_thread::sleep_for(chrono::milliseconds(intervalMs));
    }
    
    // 等待所有消息响应
    this_thread::sleep_for(chrono::seconds(2));
    
    logout();
    
    result.messagesSent = messagesSent_;
    result.messagesReceived = messagesReceived_;
    result.avgLatency = (messagesReceived_ > 0) ? totalLatency_ / messagesReceived_ : 0;
    result.maxLatency = maxLatency_;
    result.success = true;
    
    return result;
}




void StressTester::receiveThread() {
    char buffer[4096];
    
    while (isRunning_ && clientfd_ > 0) {
        int len = recv(clientfd_, buffer, sizeof(buffer), 0);
        if (len <= 0) {
            cerr << "StressTester: connection closed for user " << userId_ << endl;
            isRunning_ = false;
            break;
        }
        
        // 处理接收到的消息
        json js = json::parse(buffer);
        int msgType = js["msgid"].get<int>();
        
        switch (msgType) {
            case LOGIN_MSG_ACK: {
                if (0 == js["errno"].get<int>()) {
                    isLoggedIn_ = true;
                }
                sem_post(&loginSem_);
                break;
            }
            case ONE_CHAT_MSG: {
                messagesReceived_++;
                
                // 计算消息往返延迟
                if (js.contains("sendTime")) {
                    auto sendTime = chrono::steady_clock::time_point(
                        chrono::microseconds(js["sendTime"].get<long>()));
                    auto now = chrono::steady_clock::now();
                    double latency = chrono::duration_cast<chrono::milliseconds>(
                        now - sendTime).count();
                    recordLatency(latency);
                }
                break;
            }
            // 可以添加其他消息类型的处理
            default:
                break;
        }
    }
}

void StressTester::atomic_add(std::atomic<double>& atomic_var, double value) {
    double current = atomic_var.load();
    while (!atomic_var.compare_exchange_weak(current, current + value)) {
        // 循环直到成功
    }
}

void StressTester::recordLatency(double latency) {
    atomic_add(totalLatency_,latency);
    // totalLatency_.fetch_add(latency);
    double currentMax = maxLatency_;
    while (latency > currentMax && 
           !maxLatency_.compare_exchange_weak(currentMax, latency)) {
        // 循环直到成功更新最大值
    }
}

void StressTester::runMultiTest(const string& ip, int port, 
                              int startId, int endId, 
                              int messagesPerUser, int intervalMs,
                              int connectionsPerSecond) {
    vector<thread> threads;
    vector<TestResult> results;
    mutex resultsMutex;
    
    auto startTime = chrono::steady_clock::now();
    
    for (int userId = startId; userId <= endId; userId++) {
        // 控制连接速率
        if (connectionsPerSecond > 0 && userId > startId) {
            int elapsed = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - startTime).count();
            int expectedTime = (userId - startId) * 1000 / connectionsPerSecond;
            if (elapsed < expectedTime) {
                this_thread::sleep_for(chrono::milliseconds(expectedTime - elapsed));
            }
        }
        
        threads.emplace_back([&, userId]() {
            string password = "password" + to_string(userId);
            StressTester tester(ip, port, userId, password);
            
            TestResult result = tester.runTest(messagesPerUser, intervalMs);
            
            lock_guard<mutex> lock(resultsMutex);
            results.push_back(result);
        });
    }
    
    // 等待所有测试完成
    for (auto& t : threads) {
        t.join();
    }
    
    // 生成测试报告
    int totalSent = 0, totalReceived = 0;
    double totalLatency = 0, maxLatency = 0;
    int successCount = 0;
    
    for (const auto& result : results) {
        if (result.success) {
            successCount++;
            totalSent += result.messagesSent;
            totalReceived += result.messagesReceived;
            totalLatency += result.avgLatency * result.messagesReceived;
            if (result.maxLatency > maxLatency) maxLatency = result.maxLatency;
        }
    }
    
    double avgLatency = (totalReceived > 0) ? totalLatency / totalReceived : 0;
    
    cout << "\n=============== 压力测试报告 ===============" << endl;
    cout << "测试用户数: " << (endId - startId + 1) << endl;
    cout << "成功用户数: " << successCount << endl;
    cout << "总发送消息: " << totalSent << endl;
    cout << "总接收消息: " << totalReceived << endl;
    cout << "消息成功率: " << fixed << setprecision(2) 
         << (static_cast<double>(totalReceived) / totalSent * 100) << "%" << endl;
    cout << "平均延迟: " << avgLatency << " ms" << endl;
    cout << "最大延迟: " << maxLatency << " ms" << endl;
    cout << "=========================================" << endl;
}