#include "usermodel.hpp"
#include "connectionpool.h"
#include <iostream>
using namespace std;
//User表的增加方法
bool UserModel::insert(User &user){
    shared_ptr<MySQL> mysql = ConnectionPool::getConnectionPool()->getConnection();// 每次操作创建新连接
    //组装sql语句
    char sql[1024] = {0};
    //调用c_str()转换成char*类型
    MYSQL_STMT* stmt = mysql_stmt_init(mysql->getConnection());
    sprintf(sql,"INSERT INTO User(name,password,state) VALUES('%s','%s','%s')",
       user.getName().c_str(),user.getPwd().c_str(),user.getState().c_str());
    if(mysql){
        if(mysql->update(sql)){
            //获取插入成功的用户数据生成的主键id
            user.setId(mysql_insert_id(mysql->getConnection()));
            return true;
        }
    }
    return false;
}


User UserModel::query(int id){
    char sql[1024] = {0};
    sprintf(sql,"select * from User where id = %d",id);

    shared_ptr<MySQL> mysql = ConnectionPool::getConnectionPool()->getConnection();// 每次操作创建新连接
    if(mysql){
        MYSQL_RES *res = mysql->query(sql);
        if(res != nullptr){
            MYSQL_ROW row = mysql_fetch_row(res);
            if(row != nullptr){
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setPwd(row[2]);
                user.setState(row[3]);
                //否则内存不断泄露
                mysql_free_result(res);
                return user;
            }
        }
    }
    return User();
}

bool UserModel::updateState(User user){
    char sql[1024] = {0};
    sprintf(sql,"update User set state = '%s' where id = %d",user.getState().c_str(),user.getId());

    shared_ptr<MySQL> mysql = ConnectionPool::getConnectionPool()->getConnection();// 每次操作创建新连接
    if(mysql){
        if(mysql->update(sql))
        {
            return true;
        }
    }
    return false;
}

void UserModel::resetState(){
    char sql[1024] = "update User set state = 'offline' where state = 'online'";
    shared_ptr<MySQL> mysql = ConnectionPool::getConnectionPool()->getConnection();// 每次操作创建新连接
    if(mysql){
       mysql->update(sql);
    }
}