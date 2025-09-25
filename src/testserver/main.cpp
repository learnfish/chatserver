#include <iostream>
#include <string>
#include <vector>
#include <json.hpp>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "public.hpp"

using json = nlohmann::json;
using namespace std;

int main(int argc, char **argv) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <ip> <port> <start_id> <end_id>" << endl;
        return 1;
    }
    
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    int startId = atoi(argv[3]);
    int endId = atoi(argv[4]);
    
    for (int id = startId; id <= endId; id++) {
        int clientfd = socket(AF_INET, SOCK_STREAM, 0);
        if (clientfd == -1) {
            cerr << "Failed to create socket for user " << id << endl;
            continue;
        }
        
        sockaddr_in server;
        memset(&server, 0, sizeof(sockaddr_in));
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        server.sin_addr.s_addr = inet_addr(ip);
        
        if (connect(clientfd, (sockaddr*)&server, sizeof(sockaddr_in)) == -1) {
            cerr << "Failed to connect for user " << id << endl;
            close(clientfd);
            continue;
        }
        
        string name = "stress_user_" + to_string(id);
        string password = "password" + to_string(id);
        
        json js;
        js["msgid"] = REG_MSG;
        js["name"] = name;
        js["password"] = password;
        string request = js.dump();
        
        int len = send(clientfd, request.c_str(), request.size() + 1, 0);
        if (len <= 0) {
            cerr << "Failed to send registration for user " << id << endl;
        } else {
            cout << "Registered user: " << name << " (ID: " << id << ")" << endl;
        }
        
        close(clientfd);
        usleep(10000); // 10ms 间隔，避免服务器过载
    }
    
    return 0;
}