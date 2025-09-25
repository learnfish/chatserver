#include "offlinemessagemodel.hpp"
#include "connectionpool.h"
    //存储用户的离线消息
    void OfflineMsgModel::insert(int userid,string msg){
        //1、组装sql语句
        char sql[1024] = {0};
        sprintf(sql,"insert into Offline_Message values(%d,'%s')",userid,msg.c_str());
        shared_ptr<MySQL> mysql = ConnectionPool::getConnectionPool()->getConnection();// 每次操作创建新连接
        if(mysql){
            mysql->update(sql);
        }
    }

    //删除用户的离线消息
    void OfflineMsgModel::remove(int userid){
        char sql[1024] = {0};
        sprintf(sql,"delete from Offline_Message where userid=%d",userid);
        shared_ptr<MySQL> mysql = ConnectionPool::getConnectionPool()->getConnection();// 每次操作创建新连接
        if(mysql){
            mysql->update(sql);
        }
    }

    //查询用户的离线消息
    vector<string> OfflineMsgModel::query(int userid){
        char sql[1024] = {0};
        sprintf(sql,"select message from Offline_Message where userid=%d",userid);
        shared_ptr<MySQL> mysql = ConnectionPool::getConnectionPool()->getConnection();// 每次操作创建新连接
        vector<string> vec;
        if(mysql){
            MYSQL_RES *res = mysql->query(sql);
            if(res != nullptr){
                
                //把userid用户的所有离线消息放入vec中返回
                MYSQL_ROW row;
                while((row = mysql_fetch_row(res)) != nullptr){
                    vec.push_back(row[0]);
                }
                mysql_free_result(res);
                return vec;
            }
        }
        return vec;
    }