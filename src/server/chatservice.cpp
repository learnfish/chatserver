#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <map>
using namespace std;



using namespace muduo;

//获取单例对象的接口函数
//静态方法在类外就不用再写static了
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
}

//注册消息以及对应的Handler操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginout,this,_1,_2,_3)});

    //连接redis服务器
    if(_redis.connect()){
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}

void ChatService::reset(){
    //把online状态的用户，设置成offline
    _userModel.resetState();
}

MsgHandler ChatService::getHandler(int msgid){

    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end()){
        //LOG_ERROR << "msgid" << msgid << "can not find handler!";
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time){
            LOG_ERROR << "msgid" << msgid << "can not find handler!";
        };
    }else{
         return _msgHandlerMap[msgid];
    }
}

void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end()){
            _userConnMap.erase(it);
        }
    }
   
    //用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    //更新用户的状态信息
    User user(userid,"","","offline");
    _userModel.updateState(user);

}


void ChatService::clientCloseEcception(const TcpConnectionPtr &conn)
{    
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); ){
            if(it->second == conn){
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }else{
               ++it; 
            }
        }
    }
   //用户注销，相当于就是下线，在redis中取消订阅通道
   _redis.unsubscribe(user.getId());

    //更新用户的状态信息
    if(user.getId() != -1){
        user.setState("offline");
        _userModel.updateState(user);
    }
}




 //处理登录业务 ORM object relation map对象关系映射  业务代码和数据库代码分开
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int id = js["id"].get<int>();
    string pwd = js["password"];
    
    User user = _userModel.query(id);
    if(user.getId() == id && user.getPwd() == pwd){
        if(user.getState() == "online"){
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2; //响应成功
            response["errmsg"] = "this account is using, input another";
            conn->send(response.dump());
        }else{

            //登录成功，记录用户连接信息,c++STL容器没有考虑过线程安全的问题，所以添加线程互斥操作
            {
                //保证临界区安全代码段
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id,conn});
            }
            //id用户登录成功后，向redis订阅channel()
            _redis.subscribe(id);

            //登录成功 //数据库的并发操作由mysql来保证
            user.setState("online");
            _userModel.updateState(user);

            json response; //会产生局部变量，每个线程栈都是隔离的不会产生冲突
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0; //响应成功
            response["id"] = user.getId();
            response["name"] = user.getName();
            //查询该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty()){
                response["offlinemsg"] = vec;
                _offlineMsgModel.remove(id);
            }
            
            //查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty()){
                vector<string> vec2;
                for(User &user:userVec){
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            //查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if(!groupuserVec.empty()){
                vector<string> groupV;
                for(Group &group:groupuserVec){
                    json grpjs;
                    grpjs["id"] = group.getId();
                    grpjs["groupname"] = group.getName();
                    grpjs["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for(GroupUser &user:group.getUsers()){
                        json js1;
                        js1["id"] = user.getId();
                        js1["name"] = user.getName();
                        js1["state"] = user.getState();
                        js1["role"] = user.getRole();
                        userV.push_back(js1.dump());
                    }
                     grpjs["users"] = userV;
                    groupV.push_back(grpjs.dump());
                }
                response["groups"] = groupV;
            }
            conn->send(response.dump());
        }
    }else{

        //该用户不存在登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1; //响应成功
        response["errmsg"] = "id or password is invaild";
        conn->send(response.dump());
    }
}
//处理注册业务 name password ,业务操作的都是数据对象
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time){
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if(state){
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0; //响应成功
        response["id"] = user.getId();
        conn->send(response.dump());
    }else{
        //注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1; //响应成功
        conn->send(response.dump());
    }
}

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end()){
            //toid在线，转发消息 服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }
    //查询toid是否在线
    User user = _userModel.query(toid);
    if(user.getState() == "online")
    {
        _redis.publish(toid,js.dump());
        return;
    }

    //toid不在线，存储离线消息
    _offlineMsgModel.insert(toid,js.dump());

}


void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    _friendModel.insert(userid,friendid);
}


//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    //存储创建的群组信息
    Group group(-1,name,desc);
    if(_groupModel.createGroup(group)){
        //存储群组创建人信息
        _groupModel.addGroup(userid,group.getId(),"creator");
    }
}

//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid =js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");
}

//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid,groupid);
    lock_guard<mutex> lock(_connMutex);
    for(int id: useridVec){
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end()){
            //转发群消息
            it->second->send(js.dump());
        }else{

           //查询toid是否在线
           User user = _userModel.query(id);
           if(user.getState() == "online"){
              _redis.publish(id,js.dump());
           }else
           {
              //存储离线群消息
             _offlineMsgModel.insert(id,js.dump());            
           }

        }
    }
}

//从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid,string msg){

    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    //存储该用户的离线消息
    _offlineMsgModel.insert(userid,msg);
}