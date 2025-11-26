//https://www.baeldung.com/linux/process-daemon-service-differences
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netdb.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <signal.h>
#include <systemd/sd-daemon.h>
#include <unistd.h> 
#include <time.h>
#include <sys/epoll.h> 
#define MAX_EVENTS 1 
#define MAX_BUF    1024 
struct epoll_event listenEv;
struct epoll_event listenEvs[1];
int stats=0, statsNow=0;
const int BACK_LOG = 100;
struct dataBase
{
    int listenfd;                   //TCPfd
    int udpfd;                      //UDPfd
    int soksFd;                     //sok epoll TCP or UDP
    int epfd;                       //TCP messages
    struct sockaddr_storage addrs;  //TCP clients addrs
};
int takeOff(struct dataBase *db);   //creating structures
int cruise(struct dataBase *db);    //Program working
int landing(struct dataBase *db);   //Program finishing
void getTime(char* buff);
void getStats(char* buff);
int Analise(char* buff, struct dataBase *db, int* printly);

int signal_captured = 0;
void signal_handler(int signum, siginfo_t *info, void *extra)
{
    signal_captured = 1;
}
void set_signal_handler(void)
{
    struct sigaction action;

    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = signal_handler;
    sigaction(SIGTERM, &action, NULL);
}

int main()
{
    set_signal_handler();
    sd_notify(0, "READY=1\nSTATUS=Running");
    struct dataBase db;
    if (takeOff(&db)!=-1){
        cruise(&db);
        landing(&db);
    }
    else
        landing(&db);
    sd_notify(0, "STOPPING=1");
    return 0;
}

int takeOff(struct dataBase *db)
{
    db->listenfd=socket(AF_INET, SOCK_STREAM, 0);
    if (db->listenfd==-1)
    {
        printf("Error in creating TCP socket");
        close(db->listenfd);
        return -1;
    }
    db->udpfd = socket(AF_INET, SOCK_DGRAM, 0); 
    if (db->udpfd==-1)
    {
        printf("Error in creating UDP socket");
        close(db->listenfd);
        close(db->udpfd); 
        return -1;
    }
    struct sockaddr_in serv_addr; 
    int portno = 7000; 
    serv_addr.sin_family = AF_INET; // don't care IPv4 or IPv6 
    serv_addr.sin_addr.s_addr = INADDR_ANY; // receive packets destined to any of the interfaces 
    serv_addr.sin_port = htons(portno);   

    // by default, system doesn't close socket, but set it's status to TIME_WAIT; allow socket reuse    
    int yes = 1; 
    if (setsockopt(db->listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) { 
        printf("Error in setsockopt");
        close(db->listenfd);
        close(db->udpfd); 
        return -1;
    }  

    if (setsockopt(db->udpfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) { 
        printf("Error in setsockopt");
        close(db->listenfd);
        close(db->udpfd); 
        return -1;
    }  

    if (bind(db->listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) { 
        printf("Bind error");
        close(db->listenfd);
        close(db->udpfd); 
        return -1;
    } 

    if (bind(db->udpfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1){ 
        printf("Bind UDP error");
        close(db->listenfd);
        close(db->udpfd); 
        return -1;
    }

    struct epoll_event listenEv[2];
    listenEv[0].data.fd=db->listenfd;
    listenEv[0].events=EPOLLIN;

    listenEv[1].data.fd=db->udpfd;
    listenEv[1].events=EPOLLIN;

    db->soksFd=epoll_create(2);
    epoll_ctl(db->soksFd, EPOLL_CTL_ADD, db->listenfd, (struct epoll_event*)&listenEv[0]);
    epoll_ctl(db->soksFd, EPOLL_CTL_ADD, db->udpfd, (struct epoll_event*)&listenEv[1]);

    if (listen(db->listenfd, 1) == -1) { 
        printf("listen error"); 
        close(db->listenfd);
        close(db->udpfd); 
        return -1;
    }

    db->epfd = epoll_create(5); //TCP accs
    if (db->epfd == -1) { 
        printf("epoll_create");
        close(db->listenfd);
        close(db->udpfd); 
        return -1;
    } 
}

int cruise(struct dataBase *db)
{
    struct epoll_event socksEvs[2];
    struct epoll_event TCPEvs[5];
    socklen_t addr_size = sizeof(db->addrs);
    char buff[1000];
    while (!signal_captured){
        int socks_size=epoll_wait(db->soksFd, (struct epoll_event*)socksEvs, 2, 500);
        if(socks_size==-1)
        {
            signal_captured=1;
        }
        int TCPSize=epoll_wait(db->epfd, (struct epoll_event*)TCPEvs, 5, 500);
        if (TCPSize==-1)
        {
            signal_captured=1;
        }
        for (int i=0;i<socks_size;++i)
        {
            if (socksEvs[i].data.fd==db->listenfd)
            {
                int cs;
                if ((cs=accept(db->listenfd, (struct sockaddr*)&db->addrs, &addr_size))==-1)
                {
                    signal_captured=1;
                }
                else 
                {
                    struct epoll_event TCPev;
                    TCPev.data.fd=cs;
                    stats++;
                    statsNow++;
                    TCPev.events=EPOLLIN|EPOLLHUP;
                    if (epoll_ctl(db->epfd, EPOLL_CTL_ADD, cs, (struct epoll_event*)&TCPev)==-1)
                    {
                        signal_captured=1;
                    }
                }
            }
            else if(socksEvs[i].data.fd==db->udpfd)
            {
                int printly=0;
                struct sockaddr_in addr;
                int socklen=sizeof(addr);
                memset(buff, 0, 1000);
                if (recvfrom(socksEvs[i].data.fd, buff, 1000, 0, (struct sockaddr*)&addr, &socklen)==-1)
                {
                    //errorWork(&db);
                }
                else
                {
                    Analise(buff, db, &printly);
                    if (printly){
                        if (sendto(socksEvs[i].data.fd, buff, 1000, 0, (struct sockaddr*)&addr, socklen)==-1)
                        {
                            perror("[ERROR]: Error with UDP sending data.\n");
                        }
                    }
                }
                memset(buff, 0, 1000);
            }
        }
        for (int i=0;i<TCPSize;++i)
        {
            int printly=0;
            memset(buff, 0, 1000);
            if (recv(TCPEvs[i].data.fd, buff, 1000, 0)==-1)
            {
                //errorWork(&db);
            }
            else{
                if (TCPEvs[i].events==EPOLLIN|EPOLLRDHUP&&strlen(buff)==0)  //Закрытие сокета при наличии сигнала со стороны клиента Ctrl+d
                {
                    epoll_ctl(db->epfd, EPOLL_CTL_DEL, TCPEvs[i].data.fd, &TCPEvs[i]);
                    close(TCPEvs[i].data.fd);
                    --stats;
                }
                Analise(buff, db, &printly);
                if (printly)
                {
                    if (send(TCPEvs[i].data.fd, buff, 1000, 0)==-1)
                    {
                        //errorWork(&db);
                    }
                }
            }
            memset(buff, 0, 1000);
        }
    }
}

int Analise(char* buff, struct dataBase *db, int* printly)
{
    if ((strlen(buff)!=0)){
        if (!strncmp(buff, "/shutdown", 9))
            {
                *printly=0;
                signal_captured=1;
            }
            else if (!strncmp(buff, "/stats", 6))
            {
                *printly=1;
                memset(buff, 0, 1000);
                getStats(buff);
            }
            else if (!strncmp(buff, "/time", 5))        //GetTime
            {
                *printly=1;
                memset(buff, 0, 1000);
                getTime(buff);
            }
            
            else{
                *printly=1;
            }
    }
}
int landing(struct dataBase *db)
{
    if (close(db->epfd)==-1)
    {
        perror("[ERROR]: Error closing clientEvfd.\n");
    }
    
    if (close(db->listenfd)==-1)
    {
        perror("[ERROR]: Error closing TCP server.\n");
    }

    if (close(db->udpfd))
    {
        perror("[ERROR]: Error closing UDP server.\n");
    }

    if (close(db->soksFd)==-1)
    {
        perror("[ERROR]: Error closing listenEvfd.\n");
    }
}

void getTime(char* buff)
{
    time_t now=time(NULL);
    struct tm *t=localtime(&now);
    memset(buff, 0, 1000);
    sprintf(buff, "[INFO]: %4d-%2d-%2d %02d:%02d:%02d\n", t->tm_year+1900, t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}

void getStats(char* buff)
{
    memset(buff, 0, 1000);
    sprintf(buff, "[INFO]: number of connected clients: %d. Number of currently connected clients:%d\n", stats, statsNow);
}
//char* execDataTCP(char* buff, struct epoll_event* event)
