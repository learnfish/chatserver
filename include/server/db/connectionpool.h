#ifndef _CONNECTIONPOOL_H
#define _CONNECTIONPOOL_H
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <iostream>
#include <condition_variable>
#include "db.h"
using namespace std;

#define LOG(str) \
    std::cout << __FILE__ << ":" << __LINE__ << " " << \
    __TIMESTAMP__ << ":" << str << std::endl;

class ConnectionPool{
public:
    //返回一个单例对象
    static ConnectionPool* getConnectionPool();
    //给外部提供接口，从连接池获取一个可用的空闲链接
    shared_ptr<MySQL> getConnection();
private:
    static ConnectionPool pool;
    ConnectionPool();
    ~ConnectionPool();
    //从配置文件中加载配置项
    bool loadConfigFile();  

    //运行在独立的线程中，专门负责生产新连接
    void produceConnectionTask();
    
    void scannerConnectionTask();

    string _ip;  //mysql的IP地址
    unsigned short _port; //mysql的端口号
    string _username; //mysql登录用户名
    string _password; //mysql登录密码
    string _dbname;
    int _initSize; //连接池的初始连接量
    int _maxSize; //连接池最大连接量
    int _maxIdleTime; //连接池最大空闲时间
    int _connectionTimeout; //连接池的超时时间

    queue<MySQL*> _connectionQue; //存储mysql连接的队列
    mutex _queueMutex;  //维护连接队列的线程安全互斥锁
    atomic_int _connectionCnt;  //记录连接所创建的connection连接的总数量
    condition_variable cv;  //设置条件变量，用于连接生产线程和连接消费线程的通信
};





#endif
