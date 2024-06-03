// file: epoll_loop_event.c
// desc: ublocked IO based epoll
//      so called epool-react-heap model; epoll-events-mode. 
// 2024/06/03

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define MAX_EVENTS 1024 // max number to listen.
#define BUFFLEN 4096
#define SERV_PORT 9000

void recvdata(int fd, int events, void*arg);
void senddata(int fd, int events, void*arg);

// struct to inlucde fd and related data
struct  myevent_s
{
    int fd;         // fd to monitor
    int events;     // event to monitor
    void *arg;      // generalized parameter
    void (*call_back)(int fd, int events, void *arg);   // callback function
    int status;     // is monitored: 1: in the red-black tree(was monitored); 0: not monitored
    char buf[BUFFLEN];
    int len; // the buf size.
    long last_active; // record the time of every adding to rb-tree g_efd.
};

int g_efd;  //global var, save the return fd of epoll_crate()
struct myevent_s g_events[MAX_EVENTS+1]; //  array, +1-->listen fd.

/* init the myevent_s elements */
// eventset(&g_events[MAX_EVENTS], lfd, accetpconn, &g_events[MAX_EVENTS]);
void eventset(struct myevent_s *ev, int fd, void (*callback)(int, int, void*), void *arg)
{
    ev->fd = fd;
    ev->call_back = callback;
    ev->events = 0;
    ev->arg = arg;
    ev->status = 0;
    memset(ev->buf, 0, sizeof(ev->buf)); // or bzero();
    ev->len = 0;
    ev->last_active = time(NULL);

    return;
}


/* The event argument describes the object linked to the file descriptor fd.  The struct epoll_event is defined as :
typedef union epoll_data {
    void        *ptr;
    int          fd;
    uint32_t     u32;
    uint64_t     u64;
} epoll_data_t;

struct epoll_event {
    uint32_t     events;      // Epoll events /
    epoll_data_t data;        // User data variable /
}; */ 
// add a fd into the monitored RD-tree.
// eventadd(efd, EOPLLIN, &g_events[MAX_EVENTS]);
void eventadd(int efd, int events, struct myevent_s *ev)
{
    struct epoll_event epv = {0, {0}};
    int op;
    epv.data.ptr =ev;
    epv.events = ev->events = events; // EPOLLIN Or EOPLLOUT
    if(ev->status == 0)
    {
        op = EPOLL_CTL_ADD; // add it to the rb-tree and change the status
        ev->status = 1;
    }

    if(epoll_ctl(efd, op, ev->fd, &epv) < 0){ // add to the rb-tree
        printf("event add failed [fd=%d] ,events[%d]\n", ev->fd, events);
    } else {
        printf("event add OK [fd=%d], op=%d, events[%0X]\n", ev->fd, op, events);
    }
    return;
}

void eventdel(int efd, struct myevent_s *ev)
{
    struct epoll_event epv = {0, {0}};
    int op;
    epv.data.ptr =ev;
    epv.events = ev->events; // EPOLLIN Or EOPLLOUT
    if(ev->status == 1)
    {
        op = EPOLL_CTL_DEL; // delete it from the rb-tree and change the status
        ev->status = 0;
    }

    if(epoll_ctl(efd, op, ev->fd, &epv) < 0){ // add to the rb-tree
        printf("event del failed [fd=%d] ,events[%d]\n", ev->fd, ev->events);
    } else {
        printf("event deleted OK [fd=%d], op=%d, events[%0X]\n", ev->fd, op, ev->events);
    }
    return;
}

void acceptconn(int lfd, int events, void *arg)
{
    struct sockaddr_in cin; // client addr
    socklen_t len = sizeof(cin);
    int cfd, i;
    if((cfd = accept(lfd, (struct sockaddr*)&cin, &len))== -1){
        if(errno != EAGAIN && errno != EINTR){
            // not process now.
        }
        printf("%s: accept, %s\n", __func__, strerror(errno));
        return;
    }
    do {
        for(i = 0; i < MAX_EVENTS; ++i) {  // find a free postion in g_events array;
            if(g_events[i].status == 0)  // similar to find -1 in select
                break;
        }
        if(i == MAX_EVENTS){
            printf("%s: max connect limit[%d]\n", __func__, MAX_EVENTS);
            break;
        }
        int flag = 0;
        if((flag = fcntl(cfd, F_SETFL, O_NONBLOCK))<0){
            printf("%s: fcntl nonblocking failed, %s\n", __func__, strerror(errno));
            break;
        }

        // set myevent_s struct for cfd and set the callback to recvdata.
        eventset(&g_events[i], cfd, recvdata, &g_events[i]);  // init the cfd.
        eventadd(g_efd, EPOLLIN, &g_events[i]);
    } while(0); // only execute one time 

    printf("new connection [%s:%d][time:%ld], posp[%d]\n",
        inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), g_events[i].last_active, i);
    
    return ;
}

void recvdata(int fd, int events, void* arg)
{
    struct myevent_s *ev = (struct myevent_s *)arg;
    int len;

    // ssize_t recv(int sockfd, void *buf, size_t len, int flags); flag : 0 -> let the recv same as read()

    len = recv(fd, ev->buf, sizeof(ev->buf), 0); // read from fd, an save the data into ev->buf
    eventdel(g_efd, ev); // remove ev from the rb-tree.
    if(len > 0)
    {
        ev->len = len;
        ev->buf[len] = '\0';  // add the string end indicator
        printf("C[%d]:%s\n", fd, ev->buf);  // just print the buf and then send back to client.

        eventset(ev, fd, senddata, ev); // set the fd's callback to senddata
        eventadd(g_efd, EPOLLOUT, ev);  // add the fd into rb-tree and monitor its writing event again.
    }else if(len == 0){
        close(ev->fd); // close the connection
        // addr substract: ev - g_events = elem_position
        printf("fd=%d pos[%ld], closed\n", fd, ev-g_events);
    }else{
        close(ev->fd); // close the connection
        // addr substract: ev - g_events = elem_position
        printf("recv[fd=%d] error[%d]:%s\n", fd, errno, strerror(errno));        
    }

    return;
}

void senddata(int fd, int events, void* arg)
{
    struct myevent_s *ev = (struct myevent_s *)arg;
    int len;

    len = send(fd, ev->buf, ev->len, 0); // read from fd, an save the data into ev->buf
    eventdel(g_efd, ev); // remove ev from the rb-tree.

    if(len>0)
    {
        printf("send[fd=%d], [%d]%s\n",fd, len, ev->buf);
        eventset(ev, fd, recvdata, ev);  // change the callback back to recvdata.
        eventadd(g_efd, EPOLLIN, ev);
    } else {
        close(ev->fd);
        printf("send[fd=%d] error %s\n", fd, strerror(errno));
    }
    return;
}

void initlistensocket(int efd, int port)
{
    struct sockaddr_in sin;

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(lfd, F_SETFL, O_NONBLOCK);   // set socket to non-block

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    bind(lfd, (struct sockaddr *)&sin, sizeof(sin));

    listen(lfd, 20);  // set the backlog

    // void eventset(struct myevent_s *ev, int fd, void (*callback)(int, int, void*), void *arg)
    eventset(&g_events[MAX_EVENTS], lfd, acceptconn, &g_events[MAX_EVENTS]); // set the last element in the array.

   // eventadd(int efd, int events, struct myevent_s *ev)
    eventadd(efd, EPOLLIN, &g_events[MAX_EVENTS]);

    return;   
}

int main(int argc, char* argv[])
{
    unsigned int port = SERV_PORT;
    if(argc == 2)
        port = atoi(argv[1]);
    
    g_efd = epoll_create(MAX_EVENTS + 1); // create rb-tree, return to g_efd.
    if(g_efd < 0){
        printf("create efd in %s err:%s\n", __func__, strerror(errno));
    }

    initlistensocket(g_efd, port); // init the monitor socket

    struct epoll_event events[MAX_EVENTS + 1]; // array to save the specific event fd
    printf("Server running:port[%d]\n", port);

    int checkpos =0, i;
    while(1)
    {
        // timeout check. check 100 connections everytime.
        // if a client don't have communication with Server in 60s, then close the client connection.
        long now = time(NULL);
        for(i=0; i< 100; ++i, ++checkpos)
        {
            if(checkpos == MAX_EVENTS) {
                checkpos = 0;
            }
            if(g_events[checkpos].status != 1)  // not in the rb-tree
                continue;

            long duration = now - g_events[checkpos].last_active; // 
            if(duration >=60){
                close(g_events[checkpos].fd); // close this connection
                printf("fd=%d timeout\n", g_events[checkpos].fd);
                eventdel(g_efd, &g_events[checkpos]); // remove the client from rb-tree g_efd
            }
        }
        // monitor the rb-tree. add quilified events into event array. 1s no event return 0.
        int nfd = epoll_wait(g_efd, events, MAX_EVENTS+1, 1000); // will return 0 if no event in 1s.
        if(nfd < 0){
            printf("epoll wait error, exit\n");
            continue;
        }

        for(i = 0; i < nfd; ++i)
        {
            // use customized struct to save the void *ptr of union data.
            struct myevent_s *ev = (struct myevent_s *)events[i].data.ptr;

            /* The event argument describes the object linked to the file descriptor fd.  The struct epoll_event is defined as :
            typedef union epoll_data {
                void        *ptr;
                int          fd;
                uint32_t     u32;
                uint64_t     u64;
            } epoll_data_t;

            struct epoll_event {
                uint32_t     events;      // Epoll events /
                epoll_data_t data;        // User data variable /
            }; */        
            if((events[i].events & EPOLLIN) && (ev->events & EPOLLIN)) {//ready to read
                ev->call_back(ev->fd, events[i].events, ev->arg);  //recvdata 
            }
            if((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT)) {//ready to write
                ev->call_back(ev->fd, events[i].events, ev->arg); // senddata
            }

        }

    }

    return 0;
}
