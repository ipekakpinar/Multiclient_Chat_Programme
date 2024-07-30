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
#include <mutex>
#include <unordered_map>
#include <cstring>
#include <thread>
#include <cstdarg>

#define BUF_SIZE 1024
#define MAX_CLNT 256
#define TIMEOUT_SECONDS 20

int client_count = 0;
std::mutex mtx;
std::unordered_map<std::string, int> clients_;

void handle_client(int client_sock);
void send_msg(int client_sock, const std::string &msg);
void error_handling(const std::string &message);

void error_handling(const std::string &message) {
    std::cerr << message << std::endl;
    exit(1);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        error_handling(" kullanim : port no");
    }

    int port = atoi(argv[1]);

    int listening_sock, client_sock;
    struct sockaddr_in listening_addr, client_addr;
    socklen_t client_addr_size;

    listening_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listening_sock == -1) {
        error_handling("socket() basarisiz");
    }

    memset(&listening_addr, 0, sizeof(listening_addr));
    listening_addr.sin_family = AF_INET;
    listening_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listening_addr.sin_port = htons(port);

    if (bind(listening_sock, (struct sockaddr*)&listening_addr, sizeof(listening_addr)) == -1) {
        error_handling("bind() basarisiz");
    }

    if (listen(listening_sock, MAX_CLNT) == -1) {
        error_handling("listen() basarisiz");
    }

    

    while (1) {
        client_addr_size = sizeof(client_addr);
        client_sock = accept(listening_sock, (struct sockaddr*)&client_addr, &client_addr_size);
        if (client_sock == -1) {
            error_handling("accept() hatasi");
        }

        mtx.lock();
        client_count++;
        mtx.unlock();

        std::thread th(handle_client, client_sock);
        th.detach();
    }
    close(listening_sock);
    return 0;
}

void handle_client(int client_sock) {
    char buffer[BUF_SIZE];
    bool flag = false;
    std::string client_name;

    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(client_sock, &read_fds);

    
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    
    int select_res = select(client_sock + 1, &read_fds, NULL, NULL, &timeout);
    if (select_res == -1) {
        error_handling("select() error");
        close(client_sock);
        return;
    } else if (select_res == 0) {
        
        std::string timeout_msg = "süre aşımı";
        send_msg(client_sock, timeout_msg);
        close(client_sock);
        return;
    }
    
    
    std::string client_list_msg = "kullanicilar:";
    for (const auto& client : clients_) {
        client_list_msg += " " + client.first;
    }
    send(client_sock, client_list_msg.c_str(), client_list_msg.length() + 1, 0);
    

    //nickname alimi
    int bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) {
        std::string received_msg(buffer);

        if (received_msg.substr(0, 9) == "nickname:") {
            client_name = received_msg.substr(9);
            mtx.lock();
            if (clients_.find(client_name) == clients_.end()) {
                clients_[client_name] = client_sock;
                std::cout << "Client giris yaptı: " << client_name << " socket " << client_sock << std::endl;
                
                std::string new_entry = "client giris yapti. " + client_name;
                int client_sck;
                for (const auto& client : clients_) {
                    client_sck = client.second;
                    send(client_sck, new_entry.c_str(), new_entry.length() + 1, 0);
                }
                
            } else {
                std::string error_msg = "Nickname baska kullanici tarafindan kullanilmakta";
                send_msg(client_sock, error_msg);
                client_count--;
                flag = true;
                close(client_sock);
                mtx.unlock();
                return;
            }
            mtx.unlock();
        } else {
            error_handling("nickname gonderilmedi");
            close(client_sock);
            return;
        }
    } else if (bytes_received == 0) {
        
        close(client_sock);
        return;

    } else {
        
        
        error_handling("nickname alinirken hata olustu(recv())");
        close(client_sock);
        return;
    }

    
    while (true) {
        bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
        if (bytes_received > 0) {
            std::string received_msg(buffer);
            send_msg(client_sock, received_msg); 
        } else if (bytes_received == 0) {
            
            mtx.lock();
            clients_.erase(client_name);
            client_count--;
            mtx.unlock();
            send_msg(client_sock, client_name + " cikis yapti.");
            std::cout << "Client cikis yaptı: " << client_name << std::endl;
            
            std::string out_ = "client cikis yapti. " + client_name;
                
            int out_sck;
            for (const auto& client : clients_) {
                    out_sck = client.second;
                    send(out_sck, out_.c_str(), out_.length() + 1, 0);
                }
            
            close(client_sock);
            return;
        } else {
            
            error_handling("data alinirken hata olustu");
            close(client_sock);
            return;
        }
    }
}

void send_msg(int client_sock, const std::string &msg) {
    mtx.lock();

    size_t sender_start = msg.find_first_of("[") + 1;
    size_t sender_end = msg.find_first_of("]");
    std::string sender_name = msg.substr(sender_start, sender_end - sender_start);

    size_t at_pos = msg.find_first_of("@");
    std::string receiver_name = msg.substr(sender_end + 1, at_pos - sender_end - 1);
    std::string message = msg.substr(at_pos + 1);

    if (receiver_name == "all") {
        for (const auto &client : clients_) {
            send(client.second, msg.c_str(), msg.length() + 1, 0);
        }
    } else {
        if (clients_.find(receiver_name) == clients_.end()) {
            std::string error_msg = "gecersiz nickname: " + receiver_name;
            send(client_sock, error_msg.c_str(), error_msg.length() + 1, 0);
        } else {
            send(clients_[receiver_name], msg.c_str(), msg.length() + 1, 0);
            send(clients_[sender_name], msg.c_str(), msg.length() + 1, 0);
        }
    }

    mtx.unlock();
}
