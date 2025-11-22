#include <sys/types.h> 
#include <sys/socket.h> 
#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/epoll.h> 
#define MAX_EVENTS 1 
#define MAX_BUF    1024 
const int BACK_LOG = 100;
int main(int argc, char** argv)
{
    int listenfd=socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd==-1)
    {
        printf("Error in creating TCP socket");
        return -1;re
    }
    int udpfd = socket(AF_INET, SOCK_DGRAM, 0); 
    if (udpfd==-1)
    {
        printf("Error in creating UDP socket");
        return -1;
    }
    struct sockaddr_in serv_addr; 
    int portno = 7000; 
    serv_addr.sin_family = AF_INET; // don't care IPv4 or IPv6 
    serv_addr.sin_addr.s_addr = INADDR_ANY; // receive packets destined to any of the interfaces 
    serv_addr.sin_port = htons(portno);   

    // by default, system doesn't close socket, but set it's status to TIME_WAIT; allow socket reuse    
    int yes = 1; 
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) { 
        printf("Error in setsockopt");
        return -1;
    }  

    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) { 
        printf("Bind error");
        close(listenfd); 
        return -1;
    } 

    if (bind(udpfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1){ 
        printf("Bind UDP error");
        close(listenfd); 
        return -1;
    }

    struct epoll_event listenEv[2];
    listenEv[0].data.fd=listenfd;
    listenEv[0].events=EPOLLIN;

    listenEv[1].data.fd=udpfd;
    listenEv[1].events=EPOLLIN;

    int soksFd=epoll_create(2);
    epoll_ctl(soksFd, EPOLL_CTL_ADD, listenfd, (struct epoll_event*)&listenEv[0]);
    epoll_ctl(soksFd, EPOLL_CTL_ADD, udpfd, (struct epoll_event*)&listenEv[1]);

    if (listen(listenfd, 1) == -1) { 
        printf("listen error"); 
        close(listenfd); 
        return -1;
    }

    // create epoll instance 
    int epfd = epoll_create(5); //TCP accs
    if (epfd == -1) { 
        printf("epoll_create"); 
        return -1;
    } 

int cs; // client socket 
struct sockaddr_storage their_addr; 
socklen_t addr_size = sizeof their_addr; 

int i=0;
char request[MAX_BUF] = ""; 
struct epoll_event evlist[MAX_EVENTS];
struct epoll_event evConnectionlist[2]; 
struct epoll_event evConnectionTCPlist[5]; 
char buff[MAX_BUF];
while (strstr(request, "close") == NULL) { 
    // wait for incoming connection 

    int readyConnections = epoll_wait(soksFd, (struct epoll_event*)&evConnectionlist, 2, 500);
    if (readyConnections == -1) { 
        perror("epoll_wait"); 
    }

    for (int i=0;i<readyConnections;++i)
    {
        if (evConnectionlist[i].data.fd==listenfd)
        {
            if ((cs = accept(listenfd, (struct sockaddr *) &their_addr, &addr_size)) == -1) { 
                perror("accept error"); 
                close(listenfd); 
                exit(1); 
            }
            else
            {
                struct epoll_event clientEv;
                clientEv.data.fd=cs;
                clientEv.events=EPOLLIN|EPOLLRDHUP;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, cs, (struct epoll_event*)&clientEv)==-1)
                {
                    perror("epoll_ctl"); 
                }
            }    
        }
        else if (evConnectionlist[i].data.fd==udpfd)
        {
            if ((recvfrom(udpfd, buff, sizeof(buff), 0,  (struct sockaddr *) &their_addr, &addr_size)) == -1) { 
                perror("accept error"); 
                close(listenfd); 
                exit(1); 
            }
            else 
            {
                printf("[UDP][DATA][SOCK %d]:%s\n",udpfd, buff);
                memset(buff, 0, MAX_BUF);
            }
        }
    }  
    int TCPReady;
    TCPReady=epoll_wait(epfd, (struct epoll_event*)&evlist, MAX_EVENTS, 500);
    if (TCPReady==-1)
    {
        perror("TCP wait"); 
    }
    else if (TCPReady>0)
    {
        for (int i=0;i<TCPReady;++i){
            recv(evlist[i].data.fd, buff, 1000, 0);
            printf("[TCP][DATA][SOCK %d]:%s\n", evlist[i].data.fd, buff);
        }
    }
}
    return 0;
}
