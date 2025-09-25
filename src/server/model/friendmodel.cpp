#include "friendmodel.hpp"
#include "connectionpool.h"
//添加好友关系
void FriendModel::insert(int userid, int friendid){
    char sql[1024] = {0};
    sprintf(sql,"insert into Friend values(%d,%d)",userid,friendid);
    shared_ptr<MySQL> mysql = ConnectionPool::getConnectionPool()->getConnection();// 每次操作创建新连接
    if(mysql){
        mysql->update(sql);
    }
}

//返回用户好友列表 frienid
vector<User> FriendModel::query(int userid){
    char sql[1024] = {0};
        sprintf(sql,"select a.id,a.name,a.state from User a inner join Friend b on b.friendid = a.id where b.userid = %d",userid);
        shared_ptr<MySQL> mysql = ConnectionPool::getConnectionPool()->getConnection();// 每次操作创建新连接
        vector<User> vec;
        if(mysql){
            MYSQL_RES *res = mysql->query(sql);
            if(res != nullptr){
                
                //把userid用户的所有离线消息放入vec中返回
                MYSQL_ROW row;
                while((row = mysql_fetch_row(res)) != nullptr){
                    User user;
                    user.setId(atoi(row[0]));
                    user.setName(row[1]);
                    user.setState(row[2]);
                    vec.push_back(user);
                }
                mysql_free_result(res);
                return vec;
            }
        }
        return vec;
}