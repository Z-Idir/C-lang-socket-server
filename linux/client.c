#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>

#define PORT "2390"
#define BACKLOG 10
#define BUFF_SIZE 20

int main(int argc,char *argv[]){
    if(argc != 2){
        printf("usage : ./client <hostname>\n");
        return 1;
    }
    struct addrinfo hints,*res,*iter;
    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; 
    int sock,status;
    if((status = getaddrinfo(argv[1],PORT,&hints,&res)) != 0){
        fprintf(stderr,"failed to get the addresse info for hostname : %s, error :%s\n",argv[1],gai_strerror(status));
        freeaddrinfo(res);
        return 1;
    }
    for (iter = res;iter != NULL; iter = iter->ai_next){
        struct sockaddr_in *ipv4;
        struct sockaddr_in6 *ipv6;
        sock = socket(iter->ai_family,iter->ai_socktype,iter->ai_protocol);
        if(sock <= 0){
            continue;
        }else{
            break;
        }

    }
    if(sock <=0){
        fprintf(stderr,"failed to create socket, error :%d\n",sock);
        freeaddrinfo(res);
        return 1;
    }
    if((status = connect(sock,iter->ai_addr,iter->ai_addrlen))!=0){
        fprintf(stderr,"failed to connect, error :%d\n",status);
        freeaddrinfo(res);
        return 1;
    }
    char msg[BUFF_SIZE];
    recv(sock,msg,BUFF_SIZE,0);
    printf("message received: %s\n",msg);

}