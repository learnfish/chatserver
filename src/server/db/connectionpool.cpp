#include "connectionpool.h"
#include "json.hpp" 
#include <fstream>
#include <iostream>
using json = nlohmann::json;

//线程安全的懒汉单例函数接口
ConnectionPool* ConnectionPool::getConnectionPool(){
    //对于静态局部变量的初始化，由编译器自动进行lock和unlock
    //静态局部变量第一次运行到它的时候才进行初始化
    static ConnectionPool pool;
    return &pool;
}


bool ConnectionPool::loadConfigFile(){
        _ip = "192.168.149.131";
        _port=3306;
        _username="chat_user";
        _password="StrongPassword123!";
        _dbname="chat";
        _initSize= 10;
        _maxSize=1024;
        _maxIdleTime= 3600000;
        _connectionTimeout= 5000;  
        return true;
}

ConnectionPool::ConnectionPool()
{
    //加载配置项
    if(!loadConfigFile()){
        return;
    }
    //创建初始数量的连接
    for(int i = 0; i  < _initSize; ++i){
        MySQL *p = new MySQL(_ip, _port, _username, _password, _dbname);
        p->connect();
        p->refreshAliveTime();  // 刷新一下开始空闲的起始时间
        _connectionQue.push(p);
        _connectionCnt++;
    }

    //启动一个生产者线程
    //给这个成员方法绑定一个当前对象，this指针
    thread produce(std::bind(&ConnectionPool::produceConnectionTask,this));
    produce.detach();
    // 启动一个新的定时线程，扫描多余的空闲连接，超过maxidletime的空闲连接，进行多余的连接回收
    thread scanner(std::bind(&ConnectionPool::scannerConnectionTask,this));
    scanner.detach();
}


ConnectionPool::~ConnectionPool()
{
    unique_lock<mutex> lock(_queueMutex);
    while(!_connectionQue.empty()) {
        MySQL* p = _connectionQue.front();
        _connectionQue.pop();
        delete p;
    }
    _connectionCnt = 0;
}


//运行在独立的线程中，
void ConnectionPool::produceConnectionTask(){

    for(;;){
        unique_lock<mutex> lock(_queueMutex);
        while(!_connectionQue.empty()){
            cv.wait(lock);  //队列不空，此处生产线程进入等待状态
        }
        //连接数量没有到达上限，专门负责生产新的连接
        if(_connectionCnt < _maxSize){

            MySQL *p = new MySQL(_ip, _port, _username, _password, _dbname);
            p->connect();
            p->refreshAliveTime();  // 刷新一下开始空闲的起始时间
            _connectionQue.push(p);
            _connectionCnt++;
        }  
            
        //通知消费者线程可以消费连接了
        cv.notify_all();
    }
}


shared_ptr<MySQL> ConnectionPool::getConnection(){

    unique_lock<mutex> lock(_queueMutex);
    while(_connectionQue.empty()){
        //超过这个时间还没有，如果这个时间内有可以直接拿到结果，sleep for就是直接睡那么长时间
        if(cv.wait_for(lock,chrono::milliseconds(_connectionTimeout)) == cv_status ::timeout){
            if(_connectionQue.empty())
            {
                LOG("获取空闲连接超时了...获取连接失败")
                return nullptr;
            }
        }
    }
    //shared_ptr智能指针析构时，会把connection资源直接delete掉，相当于
    //调用MySQL的析构函数，MySQL就被close掉了
    //这里需要自定义shared_ptr的自由释放方式，把connection直接归还到queue当中
    shared_ptr<MySQL> sp(_connectionQue.front(),
    [&](MySQL* pcon){
        //这里是在服务器应用线程中调用的，所以一定要考虑队列的线程安全
        unique_lock<mutex> lock(_queueMutex);
        pcon->refreshAliveTime();  // 刷新一下开始空闲的起始时间
        _connectionQue.push(pcon);
        cv.notify_all(); 
    });

    _connectionQue.pop();
    if(_connectionQue.empty()){
        cv.notify_all();  //谁消费了队列中的最后一个connection,谁负责通知一下生产者生产连接
    }
    return sp;
}


void ConnectionPool::scannerConnectionTask()
{
    for(;;)
    {
        //通过sleep模拟定时效果
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));
        //扫描整个队列，释放多余的连接
        unique_lock<mutex> lock(_queueMutex);
        while(_connectionCnt > _initSize){
            MySQL *p = _connectionQue.front();
            if(p->getAliveTime() >= (_maxIdleTime*1000)){
                _connectionQue.pop();
                delete p;
                _connectionCnt--;
            }else{
                break;  //队头都没超过_maxIdleTime,其他肯定也没超过
            }
        }
    }
}