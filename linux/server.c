/* this is a very basic server setup, all it does is create the socket , listen and wait for connections and then accept , when it accepts it sends a message to the client and dies
* fairly simple 
* 
*/
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
int main(int argc, char *argv[] ){
    struct addrinfo hints, *res,*p;
    int status; // will hold the result of getaddrinfo
    char ipstr[INET6_ADDRSTRLEN]; // will hold the printable ip string
    int sock = 0; //this will hold the file descriptor od the socket
    
    //now we need to zero out the hints structure since we dont want any garbage values in there
    memset(&hints,0,sizeof hints);
    // here we need to fill the hints with the desired af and socktype
    hints.ai_family = AF_UNSPEC; // ip version agnostic
    hints.ai_socktype = SOCK_STREAM; // will use TCP
    hints.ai_flags = AI_PASSIVE; //fill in the ip address for me making it use the local address
    // we call the getaddrinfo with error checking ofc
    if((status = getaddrinfo(NULL,PORT,&hints,&res))!=0){ // we put NULL for node because ai_flags is set to passive meaning the ip will get filled auto (local @)
        fprintf(stderr,"getaddrinfo : %s\n",gai_strerror(status));
        return 1;
    }
    for(p = res; p != NULL; p = p->ai_next){
        struct sockaddr_in *ipv4;
        struct sockaddr_in6 *ipv6;
        void * addr;
        if((sock = socket(p->ai_family,p->ai_socktype,p->ai_protocol))<=0){
            fprintf(stderr,"failed creating socket trying next address ...\n");
            continue;
        }else{
            if (p->ai_family == AF_INET){
                ipv4 = (struct sockaddr_in *) p->ai_addr;
                addr = &(ipv4->sin_addr);
            }else{
                ipv6 = (struct sockaddr_in6 *) p->ai_addr;
                addr = &(ipv6->sin6_addr);
            }
            
            inet_ntop(p->ai_family,addr,ipstr,sizeof ipstr);
            printf("the ip addresse socket created to %s\n",ipstr);
            break; // we break out the moment we get a valid socket created
        }
    }
    if(sock<=0){
        fprintf(stderr,"failed creating socket with all available addresses, error : %s\n",strerror(errno));
        freeaddrinfo(res);
        return 1;
    }
    printf("created socket successfully\n");
    // we attempt to bind to the address
    if(bind(sock,p->ai_addr,p->ai_addrlen) != 0){
        fprintf(stderr,"failed to bind socket, error : %s\n",strerror(errno));
        close(sock);
        freeaddrinfo(res);
        return 1;
    }
    printf("socket bound successfully\n");
    if((status = listen(sock,BACKLOG)) != 0){
        fprintf(stderr,"failed to listen, error: %s\n",gai_strerror(status));
        close(sock);
        freeaddrinfo(res);
        return 1;
    }
    printf("listening on port %s\n",PORT);
    struct sockaddr_storage newAddress;
    socklen_t addrSize = sizeof newAddress;
    int newSocket = accept(sock,(struct sockaddr *)&newAddress,&addrSize);
    char ipstr[INET6_ADDRSTRLEN];
    // inet_ntop(newAddress.ss_family,((struct sockaddr *)&newAddress)->sa_data);
    if(newSocket <= 0){
        fprintf(stderr,"failed to accept a connection, error: %d\n",newSocket);
        close(sock);
        freeaddrinfo(res);
        return 1;
    }
    char *msg = "cv chwiya dina?";
    int msgLen , bytesSent;
    msgLen = strlen(msg);
    bytesSent = send(newSocket,msg,msgLen,0);
    printf("bytes sent : %d , excpected to send : %d\n",bytesSent,msgLen);
    close(newSocket);
    close(sock);
    freeaddrinfo(res);

    return 0;
}