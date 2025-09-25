#ifndef STRESS_TEST_H
#define STRESS_TEST_H
#include <string>
#include <functional>
#include <chrono>
#include <atomic>
#include <thread>
#include <vector>
#include <json.hpp>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <semaphore.h>
#include <mutex>
#include "public.hpp" // 包含你的公共头文件

using json = nlohmann::json;
using namespace std;

class StressTester {
public:
    struct TestResult {
        int userId;
        int messagesSent;
        int messagesReceived;
        double avgLatency;
        double maxLatency;
        bool success;
    };
    
    StressTester(const string& ip, int port, int userId, const string& password);
    ~StressTester();
    
    TestResult runTest(int messageCount, int intervalMs = 100);
    static void atomic_add(std::atomic<double>& atomic_val, double add);
    static void runMultiTest(const string& ip, int port, 
                            int startId, int endId, 
                            int messagesPerUser, int intervalMs = 100,
                            int connectionsPerSecond = 100);
    
private:
    void connectToServer();
    bool login();
    void logout();
    void sendMessage(int targetId, const string& msg);
    void receiveThread();
    void recordLatency(double latency);
    
    int clientfd_;
    int userId_;
    string password_;
    string serverIp_;
    int serverPort_;
    atomic_bool isRunning_{false};
    atomic_bool isLoggedIn_{false};
    thread recvThread_;
    
    // 性能统计
    atomic_int messagesSent_{0};
    atomic_int messagesReceived_{0};
    atomic<double> totalLatency_{0.0};
    atomic<double> maxLatency_{0};
    
    // 用于线程同步
    sem_t loginSem_;
};

#endif // STRESS_TESTER_H