#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <thread>

#define BUF_SIZE 1024

using namespace std;

string msg;
string nickname = "DEFAULT";

void error_handling(const std::string &message) {
    std::cerr << message << std::endl;
    exit(1);
}

void send_msg(int sock) {
    while (1) {
        getline(cin, msg);
        if (msg == "quit") {
            close(sock);
            exit(0);
        }
        string name_msg = "[" + nickname + "]" + msg; 
        send(sock, name_msg.c_str(), name_msg.length() + 1, 0);
    }
}

void recv_msg(int sock) {
    char name_msg[BUF_SIZE + nickname.length() + 1];
    while (1) {
        int str_len = recv(sock, name_msg, BUF_SIZE + nickname.length() + 1, 0);
        if (str_len == -1) {
            error_handling("mesaj aliinirken hata olustu.");
        } else if (str_len == 0) {
            cout << "Server baglantisi kapandi." << endl;
            break;
        }
        cout << string(name_msg) << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "kullsnim: server_ip port" << endl;
        exit(0);
    }

    char* serverIp = argv[1];
    int port = atoi(argv[2]);

    struct hostent* host = gethostbyname(serverIp);
    if (host == nullptr) {
        error_handling("servera olasilamadi");
    }

    sockaddr_in sendSockAddr;
    bzero((char*)&sendSockAddr, sizeof(sendSockAddr));
    sendSockAddr.sin_family = AF_INET;
    memcpy(&sendSockAddr.sin_addr, host->h_addr_list[0], host->h_length);
    sendSockAddr.sin_port = htons(port);

    int clientSd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSd < 0) {
        error_handling("socket olusturulamadi");
    }

    int status = connect(clientSd, (sockaddr*)&sendSockAddr, sizeof(sendSockAddr));
    if (status < 0) {
        error_handling("servera bağlanılamadi");
    }
    cout << "Server baglantisi kuruldu" << endl;

    cout << "nickname: ";
    getline(cin, nickname);
    string my_name = "nickname:" + nickname;
    send(clientSd, my_name.c_str(), my_name.length() + 1, 0);

    thread snd(send_msg, clientSd);
    thread rcv(recv_msg, clientSd);

    snd.join();
    rcv.join();

    close(clientSd);
    return 0;
}
