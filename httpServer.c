// file: httpServer.c
// used to resposne to the http client

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>

void * thread_func(void* arg);
void http_server_process(int sockfd, char *buff);

int create_listen_socket(unsigned short Port)
{
   int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
   if(-1 == listen_sock){
      printf("created socket failed. error:%m\n");
      return -1;
   }
   int opt = 1;
   if(setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))==-1)
   {
      printf("set sockopt failed.erro: %m\n");
      return -1;
   }
   struct sockaddr_in local = {0};
   local.sin_family = AF_INET;
   local.sin_port = htons(Port);
   local.sin_addr.s_addr= inet_addr("0.0.0.0"); // allow every ip to connect

   if(-1== bind(listen_sock, (struct sockaddr*)&local, sizeof(struct sockaddr)))
   {
      printf("bind socket failed.erro: %m\n");
      return -1;
   }

   if(-1 == listen(listen_sock, 10))
   {
      printf("listen socket failed.erro: %m\n");
      return -1;
   }
   return listen_sock;
}

int main(int argc, char const *argv[])
{
   int listen_sock = create_listen_socket(9098);
   if(-1== listen_sock){
      printf("create listen socket failed.erro: %m\n");
      return -1;
   }

   int epoll_fd = epoll_create(1);
   if(-1== epoll_fd){
      printf("create epoll failed.erro: %m\n");
      return -1;      
   }

   struct epoll_event epollEvent;
   epollEvent.events = EPOLLIN;
   epollEvent.data.fd = listen_sock;
   epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &epollEvent);
   struct epoll_event epollEventArray[256];
   while(1)
   {
      int ep_ret= epoll_wait(epoll_fd, epollEventArray, 256, 1000); // -1: block forever
      if(ep_ret < 0) break;
      else if(ep_ret == 0) continue; //timeout
      else{ // data comming
         for(int i=0; i < ep_ret; ++i){
            int sockfd = epollEventArray[i].data.fd;
            if(sockfd == listen_sock){
               // this means a client was connected to server.
               int client_sock= accept(sockfd, NULL, NULL);
               if(-1 == client_sock) continue;
               else{
                  // put the client into epoll to manage
                  memset(&epollEvent, 0, sizeof(epollEvent));
                  epollEvent.events = EPOLLIN;
                  epollEvent.data.fd = client_sock;
                  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &epollEvent);   
                  printf("New connect:%d\n", client_sock);
               }
            }
            else
            {
               char buff[4096] = {0};
               int ret = 0;
               char *pos = buff;
               while ((ret = recv(sockfd, pos, 512, 0))>0){
                  printf("%d, %s\n",ret, pos);                  
                  pos += ret;                  
                  http_server_process(sockfd, buff);        
               }       
               if(ret <=0)
               {  // connection shutdown.
                  // 1. remvoe the fd from epoll.
                  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd, NULL);
                  close(sockfd);                  
                  printf("disconnected: %d\n", sockfd);
               }
               
            }
         }  
      }
   }
   return 0;
}

void http_server_process(int sockfd, char *buff)
{
   // GET / HTTP/1.1

   // get the url

   // printf("%s\n",buff);
   char *url_begin = strchr(buff, ' ') + 1;
   char *ulr_end = strchr(url_begin, ' ');
   char url_buff[256] = {0};
 
   memcpy(url_buff, url_begin, ulr_end - url_begin);

   char fileName[256] = {0};
   if(strcmp(url_buff,"/") == 0)
   {
      // return default page index.html.
      strcpy(fileName, "index.html");
   }
   else{
      // return specified content. 
      strcpy(fileName, url_buff + 1);
   }
   // return HTTP data 
   char* http_response_temp = "HTTP/1.1 200 OK\r\nContent-Length:%d\r\n\r\n";
   FILE *fp = fopen(fileName, "rb");
   if(fp == NULL)
   {
      send(sockfd, "HTTP/1.1 404 OK\r\n\r\n", strlen("HTTP/1.1 404 OK\r\n\r\n"), 0);
      return;
   }
   fseek(fp, 0, SEEK_END);
   int file_len = ftell(fp);
   fseek(fp, 0, SEEK_SET);

   // send data head
   char http_response[512] = {0};
   sprintf(http_response, http_response_temp, file_len);
   send(sockfd, http_response, strlen(http_response), 0);
   // send file
   char file_content[512] = {0};
   while(1){
      size_t ret = fread(file_content, 1, 512, fp);
      if(ret <=0) break;
      send(sockfd, file_content, ret, 0);
   }
   fclose(fp);
}
