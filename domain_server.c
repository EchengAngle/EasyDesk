// file: domainerver.c
// date: 2024/06/20
// receive string and send back the upper string.

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <stddef.h>


#define SERV_ADDR "serv.socket"

int main(void)
{
    int lfd, cfd, len, size, i;
    struct sockaddr_un servaddr, cliaddr;
    char buf[4096];

    lfd = socket(AF_UNIX, SOCK_STREAM, 0);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_UNIX; // or AF_LOCAL
    strcpy(servaddr.sun_path, SERV_ADDR);

    // offset of the struct member. offsetof(TYPE, member);
    len = offsetof(struct sockaddr_un, sun_path) + strlen(servaddr.sun_path); /*servaddr total len*/

    unlink(SERV_ADDR);// make sure serv.sock file does not exist. bind will create this file.
    bind(lfd, (struct sockaddr *)&servaddr, len); // parm 3 should be the actual lenght of seraddr.
    listen(lfd, 20);

    printf("Accept ...\n");

    while(1)
    {
        len = sizeof(cliaddr); // AF_UNIX'size +108B
        cfd = accept(lfd, (struct sockaddr*)&cliaddr, (socklen_t *)&len);
        
        len -= offsetof(struct sockaddr_un, sun_path); // get the file name's length
        cliaddr.sun_path[len] = '\0'; // set the end of the string to make sure no mess in print
        printf("client bind filename %s\n", cliaddr.sun_path);

        while((size = read(cfd, buf, sizeof(buf))) >0)
        {
            for(i = 0; i < size; ++i)
                buf[i] = toupper(buf[i]);
            write(cfd, buf, size);
        }
        close(cfd);
    }

    return 0;
}
