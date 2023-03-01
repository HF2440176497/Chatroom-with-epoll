
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <list>

using namespace std;

#define SERVER_PORT 9000
#define CLIENT_SIZE 100
#define BUF_SIZE 200

#define CAUTION "There is only one client ! \n"
#define WELCOME "Welcome to chat room ! \n"

list<int> clients_list;
struct epoll_event epoll_list[CLIENT_SIZE];

/**
 * @brief
 * @param sockfd
 * @return int
 */
int SetnonBlocking(int sockfd) {
    int old_option = fcntl(sockfd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(sockfd, F_SETFL, old_option | O_NONBLOCK);
    return old_option;
}

/**
 * @brief 
 * @param epfd 
 * @param connfd 
 * @param cliieaddr 
 */
void addfd(int epfd, int connfd, const struct sockaddr_in &cliieaddr) {
    printf("ID: %d at port: %d has been connected... \n", connfd, ntohs(cliieaddr.sin_port));
    printf("Now we have %ld users \n", clients_list.size()+1);
    send(connfd, WELCOME, strlen(WELCOME), 0);

    struct epoll_event conn_event;
    conn_event.data.fd = connfd;
    conn_event.events = EPOLLIN | EPOLLET | EPOLLERR;

    SetnonBlocking(connfd);
    epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &conn_event);
    clients_list.push_back(connfd);
}

/**
 * @brief 向其他 fd 广播，main 中调用一次，通过返回值判断是否广播成功
 * @param
 * @return int == 0 广播成功或对端关闭 < 0 出错
 */
int BroadcastMessage(int epfd, int rfd) {
    char message_buf[BUF_SIZE];
    memset(message_buf, '\0', BUF_SIZE);      
    while (1) {
        int n = recv(rfd, message_buf, BUF_SIZE - 1, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {  // 读取完成最后进入此分支
                printf("Read alreday down. Waiting for calling later ... \n\n");
                break;  // 应当跳出 while 
            } else {
                close(rfd);
                return -1;
            }
        } else if (n == 0) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, rfd, epoll_list);
            printf("ID: %d has been closed \n", rfd);
            clients_list.remove(rfd);
            close(rfd);
            break;
        } else {
            if (clients_list.size() == 1) {
                if (send(rfd, CAUTION, strlen(CAUTION), 0) < 0)
                    return -1;
            }
            for (auto client:clients_list) {
                if (client == rfd)
                    continue;
                if (send(client, message_buf, strlen(message_buf), 0) < 0)
                    return -1;
            }
        }
    }
    return 0;
}

int main() {
    struct sockaddr_in servaddr;
    servaddr.sin_family = PF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    printf("listen socket created \n");

    int ret = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    int epfd = epoll_create(CLIENT_SIZE);
    assert(epfd != -1);

    struct epoll_event listen_event;
    listen_event.data.fd = listenfd;
    listen_event.events = EPOLLIN | EPOLLERR;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &listen_event);

    while (1) {
        int events_count = epoll_wait(epfd, epoll_list, CLIENT_SIZE, -1);

        for (int i = 0; i < events_count; i++) {
            if (epoll_list[i].data.fd == listenfd && epoll_list[i].events & EPOLLIN) {
                struct sockaddr_in clieaddr;
                socklen_t clielen = sizeof(clieaddr);
                int connfd = accept(listenfd, (struct sockaddr *)&clieaddr, &clielen);
                addfd(epfd, connfd, clieaddr);
            } else if (epoll_list[i].events & EPOLLIN) {
                if (BroadcastMessage(epfd, epoll_list[i].data.fd) < 0) 
                    printf("Broadcast error ! \n");
            } else {
                continue;
            }
        }
    }
}
