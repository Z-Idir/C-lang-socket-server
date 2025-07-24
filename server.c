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

#pragma comment(lib, "Ws2_32.lib")

int main() {
    WSADATA wsaData;
    int initResult;
    // initialize winsock and save the result in initResult,
    // MAKEWORD(2,2) makes a request for version 2.2 of winsock to the operating sys
    initResult = WSAStartup(MAKEWORD(2,2),&wsaData);
    if(initResult != 0) {
        printf("WSAStartup failed with error: %d\n",initResult);
        return 1;
    }
    struct addrinfo *result= NULL, *ptr =NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    initResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result );
    if(initResult != 0) {
        printf("getaddrinfo failed with error: %d\n",initResult);
        WSACleanup();
        return 1;
    }

    SOCKET listenSocket = INVALID_SOCKET;
    listenSocket = socket(result->ai_family, result->ai_socktype,result->ai_protocol);
    if(listenSocket == INVALID_SOCKET) {
        printf("error at socket() : %d\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    initResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if(initResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);

    if(listen(listenSocket,SOMAXCONN) == SOCKET_ERROR){
        printf("listen failed with error: %d\n",WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }
    SOCKET clientSocket = INVALID_SOCKET;
    clientSocket = accept(listenSocket,NULL,NULL);
    if(clientSocket == INVALID_SOCKET) {
        printf("error occurred at accept() with error code %d\n",WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    int sendResult;
    closesocket(listenSocket);
    do{
        // we receive from the client
        initResult = recv(clientSocket, recvbuf, recvbuflen, 0);
        if(initResult > 0){
            //we echo back what they sent
            printf("Bytes received: %d\n", initResult);
            sendResult = send(clientSocket, recvbuf, initResult, 0);
            if(sendResult ==SOCKET_ERROR) {
                printf("send failed with error: %d\n",WSAGetLastError());
                closesocket(clientSocket);
                WSACleanup();
                return 1;
            }
            printf("sent %d bytes back to client\n",sendResult);
            printf("the stuff %s\n",recvbuf);

        }else if( initResult == 0) {
            printf("it says connection is closing...(dunno why btw)\n");
        }else {
            printf("recv failed : %d\n",WSAGetLastError());
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }

    }while(initResult > 0);
    initResult = shutdown(clientSocket, SD_SEND);
    if(initResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    closesocket(clientSocket);
    WSACleanup();

  return 0;
}