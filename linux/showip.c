/* so basically all this code does is take a hostname from the command line being the second 
* argument after the .exe and return the ip addreses corresponding to it, the hostname
* is of the form www.meowmeow.com, and dont add a dash at the end it wont work :>
*/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
int main(int argc, char *argv[] ){
    struct addrinfo hints, *res,*p;
    int status; // will hold the result of getaddrinfo
    char ipstr[INET6_ADDRSTRLEN]; // will hold the printable ip string
    // since we will use the command line to intrduce the hostname we want to resolve
    if(argc != 2){
        fprintf(stderr,"usage : ./showip <hostname>\n");
        return 1;
    }
    //now we need to zero out the hints structure since we dont want any garbage values in there
    memset(&hints,0,sizeof hints);
    // here we need to fill the hints with the desired af and socktype
    hints.ai_family = AF_UNSPEC; // ip version agnostic
    hints.ai_socktype = SOCK_STREAM; // will use TCP
    // we call the getaddrinfo with error checking ofc
    if((status = getaddrinfo(argv[1],NULL,&hints,&res))!=0){ // we put NULL for service to just ignore it because we only care about the node or ip addresse
        fprintf(stderr,"getaddrinfo : %s\n",gai_strerror(status));
        return 1;
    }
    printf("the ip addresses for the hostname %s\n\n", argv[1]);
    for(p = res; p != NULL; p = p->ai_next){
        void *addr;
        char *ipver;
        struct sockaddr_in *ipv4;
        struct sockaddr_in6 *ipv6;
        if(p->ai_family == AF_INET){// the case of ipv4
            ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        }else {// ipv6
            ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }
        inet_ntop(p->ai_family,addr,ipstr,sizeof ipstr);
        printf("ip version : %s address : %s\n",ipver,ipstr);
    }
    freeaddrinfo(res);

    return 0;
}