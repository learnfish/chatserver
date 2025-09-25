#ifndef DB_H
#define DB_H
#include <mysql/mysql.h>
#include <string>
#include <ctime>
using namespace std;
//数据库操作类
class MySQL{
public:
    //初始化数据库练级
    MySQL(const std::string& ip, int port, 
          const std::string& username, const std::string& password,
          const std::string& dbname);
    //释放数据库连接资源
    ~MySQL();
    //连接数据库
    bool connect();
    //更新操作
    bool update(string sql);
    //查询操作
    MYSQL_RES* query(string sql);


    //获取连接
    MYSQL* getConnection();

    //刷新一下连接的起始的空闲时间点
    void refreshAliveTime(){_alivetime = clock();}

    clock_t getAliveTime() const {return clock() - _alivetime;}

private:
    MYSQL *_conn;
    std::string _server;
    std::string _user;
    std::string _password;
    std::string _dbname;
    int _port;
    clock_t _alivetime; //进入空闲状态后的起始存活时间
};


#endif