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
        printf("Error in creating socket");
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

    if (listen(listenfd, 1) == -1) { 
    printf("listen error"); 
    close(listenfd); 
    return -1;
    }

    // create epoll instance 
    int epfd; 
    epfd = epoll_create(5); 
    if (epfd == -1) { 
        printf("epoll_create"); 
        return -1;
    } 

int cs; // client socket 
struct sockaddr_storage their_addr; 
socklen_t addr_size = sizeof their_addr; 

int i=0;
char request[MAX_BUF] = ""; 
struct epoll_event AccEvs[MAX_EVENTS]; 
struct epoll_event AccEv;
AccEv.data.fd = listenfd; 
AccEv.events = EPOLLIN; 
if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &AccEv) == -1) { 
            perror("epoll_ctl"); 
}
while(1)
{
    printf("%d\n",i++);
    int ready = epoll_wait(epfd, AccEvs, 1, 500);
    if (ready == -1) { 
        perror("epoll_wait"); 
    } 
    if (ready>0){
        int ed=accept(listenfd, (struct sockaddr *) &their_addr, &addr_size);
        printf("%d\n", ed);
    }

}
return 0;
}
