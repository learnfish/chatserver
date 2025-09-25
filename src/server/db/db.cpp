#include "db.h"
#include <muduo/base/Logging.h>
#include <iostream>
using namespace std;


 //初始化数据库连接
MySQL::MySQL(const std::string& ip, int port, 
          const std::string& username, const std::string& password,
          const std::string& dbname)
      : _server(ip), _port(port), _user(username), 
      _password(password), _dbname(dbname)   
    {
        _conn = mysql_init(nullptr);
    }
    //释放数据库连接资源
MySQL::~MySQL(){
        if(_conn != nullptr)
            mysql_close(_conn);
    }
    //连接数据库
bool MySQL::connect(){
        MYSQL *p = mysql_real_connect(_conn,_server.c_str(),_user.c_str(),
            _password.c_str(),_dbname.c_str(),_port,nullptr,0);
        if(p != nullptr){
            //c和c++代码默认的编码字符是ASCII，如果不设置，从MySQL上拉下来的中文显示?
            mysql_query(_conn,"set names gbk");
            LOG_INFO << "connect mysql success!";
        }else{
            LOG_INFO << "connect mysql fail!";
        }
        return p;
    }

bool MySQL::update(string sql){
        if(mysql_query(_conn,sql.c_str())){
            LOG_INFO << __FILE__ << ":" << __LINE__ <<":"
                << sql << "更新失败";
            return false;
        }
        return true;
    }

    //查询操作
MYSQL_RES* MySQL::query(string sql){
        if(mysql_query(_conn,sql.c_str())){
            LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                << sql << "查询失败！";
            return nullptr;
        }
        return mysql_use_result(_conn);
    }

MYSQL* MySQL::getConnection(){
    return _conn;
}