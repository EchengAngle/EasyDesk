// file: udpclient.c
// date: 2024/06/03

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#define SERV_PORT 9000

int main(int argc, char* argv[])
{
    struct sockaddr_in servaddr;
    int sockfd, n;
    char buf[BUFSIZ];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);

    bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    while(fgets(buf, BUFSIZ, stdin) != NULL){
        n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
        if(n == -1)
            perror("sendto error");

        n = recvfrom(sockfd, buf, BUFSIZ, 0, NULL, 0); // don't care the peer's info
        if(n == -1) {
            perror("recvfrom error");
        }

        write(STDOUT_FILENO, buf, n); 
    }
    close(sockfd);
    return 0;
}
