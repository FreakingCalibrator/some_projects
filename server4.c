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
const int BACK_LOG = 100;
int stats=0;
struct dataBase
{
    int listenfd;
    struct sockaddr_in serv;
    int listenEvfd;
    struct epoll_event listenEv;
    struct epoll_event listenEvs[1];
    int clientEvfd;
};

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

int shutdown(int clientEvfd, int listenEvfd);
int flightPlan(struct dataBase *db);
int takeOff();
int getTime(int fd);
int getStats(int fd);
int main(int argc, char** argv)
{
    set_signal_handler();
    sd_notify(0, "READY=1\nSTATUS=Running");
    char buff[1000];
    char answer[100];
    struct dataBase db;
    if (flightPlan(&db)==-1)
        return -1;

    struct epoll_event clientEv;
    struct epoll_event clientEvs[5];
    struct sockaddr_storage client;
    socklen_t size = sizeof(client);
    int readSize;
    while (!signal_captured)
    {
        int acceptSize = epoll_wait(db.listenEvfd, (struct epoll_event*)&listenEvs, 1, 500);
        if (acceptSize==1)
        {
            int cs;
            if ((cs = accept(db.listenfd, (struct sockaddr*)&client, &size))==-1)
            {
                perror("[ERROR]: Error accepting connection.\n");
                close(db.listenfd);
                return -1;
            }
            else 
                stats++;

            clientEv.data.fd=cs;
            clientEv.events=EPOLLIN|EPOLLRDHUP;
            if (epoll_ctl(db.clientEvfd, EPOLL_CTL_ADD, cs, &clientEv)==-1)
            {
                perror("[ERROR]: Error with ctl new clients.\n");
                shutdown(db.clientEvfd, db.listenEvfd);
            }
        }
        else if (acceptSize==-1)
        {
            perror("Error with accept waiting.\n");
            shutdown(db.clientEvfd, db.listenEvfd);
        }
        readSize=0;
        if ((readSize=epoll_wait(db.clientEvfd, (struct epoll_event*)clientEvs, 5, 500))==-1)
        {
            perror("[ERROR]: Error with read waiting.\n");
            shutdown(db.clientEvfd, db.listenEvfd);
        }
        for (int i=0;i<readSize;++i)
        {
            if (read(clientEvs[i].data.fd, buff, 1000)==-1)
            {
                perror("[ERROR]: Error with read data.\n");
                shutdown(db.clientEvfd, db.listenEvfd);
            }
            else if (clientEvs[i].events==EPOLLIN|EPOLLRDHUP&&strlen(buff)==0)  //Закрытие сокета при наличии сигнала со стороны клиента Ctrl+d
            {
                epoll_ctl(db.clientEvfd, EPOLL_CTL_DEL, clientEvs[i].data.fd, &clientEvs[i]);
                close(clientEvs[i].data.fd);
                --stats;
            }
            else if ((strlen(buff)!=0)){
                if (!strncmp(buff, "/shutdown", 9))
                {
                    return shutdown(db.clientEvfd, db.listenEvfd);
                }
                else if (!strncmp(buff, "/stats", 6))
                {
                    int res=getStats(clientEvs[i].data.fd);
                    if (res==-1)
                        return res;
                }
                else if (!strncmp(buff, "/time", 5))        //GetTime
                {
                    int res=getTime(clientEvs[i].data.fd);
                    if (res==-1)
                        return res;
                }
                //printf("[DATA]: socket %d: %s\n", clientEvs[i].data.fd, buff);
                memset(buff, 0, 1000);
            }
        }
    }   

    return 0;
}
////////////////////////////////////////////////////////////////
int shutdown(int clientEvfd, int listenEvfd)
{
    if (close(clientEvfd))
    {
        perror("[ERROR]: Error closing clientEvfd.\n");
        sd_notify(0, "STOPPING=1");
    }
    if (close(listenEvfd))
    {
        perror("[ERROR]: Error closing listenEvfd.\n");
        sd_notify(0, "STOPPING=1");
    }
    sd_notify(0, "STOPPING=1");
}

int getTime(int fd)
{
    char answer[100];
    time_t now=time(NULL);
    struct tm *t=localtime(&now);
    sprintf(answer, "[INFO]: %4d-%2d-%2d %02d:%02d:%02d\n", t->tm_year+1900, t->tm_mon, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    if (send(fd, answer, strlen(answer), 0)==-1)
    {
        perror("[ERROR]: Error with sending time data.\n");
        return -1;
    }
    memset(answer, 0, 100);
}

int getStats(int fd)
{
    char answer[100];
    sprintf(answer, "[INFO]: number of users on the server: %d\n", stats);
    if (send(fd, answer, strlen(answer), 0)==-1)
    {
        perror("[ERROR]: Error with sending statistig data.\n");
        return -1;
        }
    memset(answer, 0, 100);
}

int flightPlan(struct dataBase *db)
{
    db->listenfd=socket(AF_INET, SOCK_STREAM, 0);
    if (db->listenfd==-1)
    {
        perror("[ERROR]: Error in creating socket.\n");
        close(db->listenfd);
        return -1;
    }
    db->serv.sin_addr.s_addr=INADDR_ANY;    //любой адрес принимает
    db->serv.sin_family=AF_INET;
    db->serv.sin_port=htons(7000);
    int yes=1;

    if (setsockopt(db->listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
    {
        perror("[ERROR]: Error in setting socket options.\n");
        close(db->listenfd);
        return -1;
    }

    if (bind(db->listenfd, (struct sockaddr*) &db->serv, sizeof(db->serv))==-1)
    {
        perror("[ERROR]: Error in binding socket.\n");
        close(db->listenfd);
        return -1;
    }

    if (listen(db->listenfd, 1)==-1)
    {
        perror("[ERROR]: Error in setting listen socket.\n");
        close(db->listenfd);
        return -1;
    }

    db->listenEvfd = epoll_create(1);
    if (db->listenEvfd==-1)
    {
        perror("[ERROR]: Error creating listenEvfd.\n");
        close(db->listenfd);
        return -1;
    }
    struct epoll_event listenEvs[1];
    db->listenEv.data.fd=db->listenEvfd;
    db->listenEv.events=EPOLLIN;
    if (epoll_ctl(db->listenEvfd, EPOLL_CTL_ADD, db->listenfd, &db->listenEv)==-1)
    {
        perror("[ERROR]: Error ctl listenEv.\n");
        close(db->listenfd);
        return -1;
    }

    db->clientEvfd = epoll_create(5);
    if (db->clientEvfd==-1)
    {
        perror("[ERROR]: Error creating clientEvfd.\n");
        close(db->listenfd);
        return -1;
    }
}

int takeOff()
{}
