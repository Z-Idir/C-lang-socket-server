#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512
#define DEFAULT_IP_ADDRESSE "127.0.0.1"



#pragma comment(lib, "Ws2_32.lib")

int main(int argc, char **argv) {
    WSADATA wsadata;
    int iResult;
    iResult = WSAStartup(MAKEWORD(2,2),&wsadata);
    if(iResult != 0) {
        printf("WSAStartup failed with error: %d\n",iResult);
        return 1;
    }
    struct addrinfo *result = NULL, *ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    char * addresse ;
    printf("arg count %d\n",argc);
    if(argc > 1){
        addresse = argv[1];
    }else {
        addresse = DEFAULT_IP_ADDRESSE;
    }
    printf("ip address %s\n",addresse);
    iResult = getaddrinfo(addresse,DEFAULT_PORT, &hints, &result);
    if(iResult != 0) {
        printf("getaddrinfo failed with error: %d\n",iResult);
        WSACleanup();
        return 1;
    }
    SOCKET connectSocket = INVALID_SOCKET;
    ptr = result;
    connectSocket = socket(ptr->ai_family,ptr->ai_socktype,ptr->ai_protocol);
    if(connectSocket == INVALID_SOCKET) {
        printf("error at socket() : %d\n",WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if(iResult == SOCKET_ERROR) {
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    if(connectSocket == INVALID_SOCKET) {
        printf("unable to connect to server error at connect() line 55 : %d\n",WSAGetLastError());
        WSACleanup();
        return 1;
    }
    const char * sendbuf = "test message";
    char recvbuf[DEFAULT_BUFLEN];
    iResult = send(connectSocket, sendbuf, (int)strlen(sendbuf),0);
    if(iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n",WSAGetLastError());
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }
    printf("bytes sent %d\n",iResult);
    iResult = shutdown(connectSocket,SD_SEND);
    if(iResult == SOCKET_ERROR) {
        printf("shutdown failed with error : %d\n",WSAGetLastError());
        closesocket(connectSocket);
        return 1;
    }
    do {
        iResult = recv(connectSocket,recvbuf,(int)strlen(recvbuf),0);
        if(iResult > 0 ){
            printf("Bytes received : %d\n",iResult);
            printf("message %s\n",recvbuf);
        }else if(iResult == 0 ){
            printf("connection closed\n",WSAGetLastError());
        }else {
            printf("recv failed with error : %d\n",WSAGetLastError());
        }
    }while(iResult > 0);
    closesocket(connectSocket);
    WSACleanup();
    return 0;
}