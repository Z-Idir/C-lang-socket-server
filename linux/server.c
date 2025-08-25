/* this is the fork per client server version where we create child processes to handle client on each connection
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
#include <stdlib.h>
#include <wait.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

#include "server.h"
#include "child_process_utils.h"

/*----CONSTANTS DEFINITIONS----*/
#define PORT "2390"
#define BACKLOG 10
#define BUFF_SIZE 100
#define MAX_CHILDREN 1
#define SOCK_TIMEOUT 60
#define CLIENT_CLOSE_MSG 2
#define MAX_QUEUE_SIZE 2

/*----FLAGS DEFINITIONS----*/
volatile sig_atomic_t running = 1;
volatile sig_atomic_t childRunning = 1;
volatile sig_atomic_t rewind_handler = 1; // this will be used to wake the process to handle enqued connections

/*----STRUCTURES DEFINITIONS----*/
typedef struct {
    int queue[MAX_QUEUE_SIZE];
    int tail;
    int head;
    int count;
} shared_queue_t ;

/*----SYNOPSIS DEFINITIONS----*/
void child_main(int ipc_sock, int index);
void handleSigInt(int sig);
void handleSigChild(int sig);
void format_client_addresse(struct sockaddr *ca, char *out, size_t outlen);
int server_should_run(void);
ssize_t send_full(int socket_fd,char *buf,size_t buflen);
ssize_t recv_full(int socket_fd,char *buf,size_t buflen);
void handle_child_sig_int(int sig);
void install_child_sig_handler(void);
void cleanup_children(void);
int response(int sock,char *request, ssize_t reqlen);
static int send_file_descriptor(int socket,int fd_to_send);
static int recv_file_descriptor(int socket);
void add_child(pid_t pid);
void enqueue_connection(shared_queue_t *q, int fd);
int dequeu_connection(shared_queue_t *q);

/*----IPC MANAGEMENT----*/
// pid_t children_pids[MAX_CHILDREN];
pl *children_pids = NULL;
int childrenCount = 0;

sem_t *mutex ,*items,*slots ;
shared_queue_t *pending_queue;

int child_index;


int main(){
    int ipc_pairs[MAX_CHILDREN][2];
    // int sock_pair[2];

    int ipc_establish_index = 0;
    // socketpair(AF_UNIX,SOCK_STREAM,0,sock_pair);
    mutex = sem_open("/mutex_sem",O_CREAT,0666,1);
    items = sem_open("/items_sem",O_CREAT,0666,0);
    slots = sem_open("/slots_sem",O_CREAT,0666,MAX_QUEUE_SIZE);


    install_sig_inter_handler();
    int sock = setup_listener(PORT);
    int smfd = shm_open("/shared_queue",O_CREAT | O_RDWR,0666);
    ftruncate(smfd,sizeof(shared_queue_t));
    pending_queue = mmap(NULL,sizeof(shared_queue_t),PROT_READ | PROT_WRITE,MAP_SHARED,smfd,0);
    pending_queue->head = 0;
    pending_queue->tail = 0;
    pending_queue->count = 0;
    memset(pending_queue->queue,-1,sizeof(int[MAX_QUEUE_SIZE]));


    while (server_should_run()){

        reap_children_non_block();
        struct sockaddr_storage newAddress; // since we dont know whether the client will connect using ipv4 or 6 we use sockaddr_storage because it is big enough to hold any
        socklen_t addrSize = sizeof newAddress;
        if (!server_should_run()) {
            printf("[debug] server_should_run() is false, breaking out of loop\n");
            break;
        }

        int newSocket = accept(sock,(struct sockaddr *)&newAddress,&addrSize);
        char ip[128];
        if(newSocket == -1){
            if(errno == EINTR && !running) break;
            if(errno == EINTR) continue;
            fprintf(stderr,"failed to accept a connection, error: %s\n",strerror(errno));
            continue;
        }
        format_client_addresse((struct sockaddr *)&newAddress,ip,sizeof ip);
        if(childrenCount < MAX_CHILDREN){
            child_index = ipc_establish_index;
            printf("going to fork anew\n");
            printf("the accepted client on fork has sock <%d>\n",newSocket);
            if(socketpair(AF_UNIX,SOCK_STREAM,0,ipc_pairs[child_index]) < -1){
                perror("socketpair");
                exit(EXIT_FAILURE);
            }
            ipc_establish_index++;
            pid_t pid = fork();
            if(pid < 0){
                perror("fork");
                close(newSocket);
            }else if(pid == 0){ // child
                close(sock); //close its copy of the server socket
                // close(ipc_pairs[ipc_establish_index - 1][0]);
                child_main(ipc_pairs[child_index][1],child_index);
                // close(ipc_pairs[ipc_establish_index - 1][1]);
                exit(0);
            }else{//parent
                add_child(pid);
                printf("spawned a child with pid :<%d>\n",pid);
                send_file_descriptor(ipc_pairs[child_index][0],newSocket);
                close(newSocket); // close its copy of the client socket
            }
        }else {
            int dupSock = dup(newSocket);
            printf("connection going into queue sock <%d>\n",dupSock);
            enqueue_connection(pending_queue, dupSock);
            printf("connection went into queue\n");
            close(newSocket);
        }

    }
    printf("\nclosing server ...\n");
    cleanup_children();
    close(sock);

    shm_unlink("/shared_queue");
    sem_unlink("/slots_sem");
    sem_unlink("/items_sem");
    sem_unlink("/mutex_sem");

    return 0;
}
void install_sig_inter_handler(void){
    // interruption handler
    struct sigaction sa_int = {0};
    sa_int.sa_handler = handleSigInt;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT,&sa_int,NULL);

    // child signal handler (when a child dies)
    struct sigaction sa_child = {0} ;
    sa_child.sa_handler = handleSigChild;
    sigemptyset(&sa_child.sa_mask);
    sa_child.sa_flags = 0;
    sigaction(SIGCHLD,&sa_child,NULL);
}
void reap_children_non_block(void){
    // this while loop will be our non-blocking reaper, the first ard -1 means wait for any child regardless of pid, status is NULL for we do not care, and flags is 
    // WNOHANG to make it non-blocking
    pid_t pid;
    while((pid = waitpid(-1,NULL,WNOHANG)) > 0){
        printf("reaping child with pid <%d>\n",pid);
        delete_node(pid,&children_pids);
        childrenCount--;
    }
}
void reap_children_blocking(void){
    while(waitpid(-1,NULL,0) > 0){}
}

int setup_listener(char *port){
    struct addrinfo hints, *res,*p;
    int status; // will hold the result of getaddrinfo
    //char ipstr[INET6_ADDRSTRLEN]; // will hold the printable ip string
    int sock; //this will hold the file descriptor od the socket
    //now we need to zero out the hints structure since we dont want any garbage values in there
    memset(&hints,0,sizeof hints);

    // here we need to fill the hints with the desired af and socktype
    hints.ai_family = AF_UNSPEC; // ip version agnostic
    hints.ai_socktype = SOCK_STREAM; // will use TCP
    hints.ai_flags = AI_PASSIVE; //fill in the ip address for me making it use the local address
    if((status = getaddrinfo(NULL,port,&hints,&res))!=0){ // we put NULL for node because ai_flags is set to passive meaning the ip will get filled auto (local @)
        fprintf(stderr,"getaddrinfo : %s\n",gai_strerror(status));
        return -1;
    }
    // here we will iterate through all the addresse info until we find one we can create the 
    // socket to and bind to it
    for(p = res;p != NULL;p = p->ai_next){
        if((sock = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1){
            fprintf(stderr,"failed creating socket trying next address ...\n");
            // try next one
            continue;
        }else{// meaning the socket was created
            // here we set it to reuse the address to avoid address already in use error when restarting the server
            int opt = 1;
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
                perror("setsockopt(SO_REUSEADDR)");
                exit(1);
            }
            //we now attempt to bind it to that address
            if((status = bind(sock,p->ai_addr,p->ai_addrlen)) != 0){
                fprintf(stderr,"failed to bind socket, error : %s\n",strerror(errno));
                // try next one
                continue;
            }else{
                // when we bind the socket successfully we stop the search
                break;
            }
        }
    }
    // we now check wether the loop above did any good
    if(p == NULL){
        if(sock == -1){
            fprintf(stderr,"failed to create socket with all available addresses, error : %s\n",strerror(errno));
        }
        else if(status == -1){
            fprintf(stderr,"failed to bind socket with all available addresses, error : %s\n",strerror(errno));
            close(sock);
        }else{
            perror("socket/bind");
        }
        freeaddrinfo(res);
        return -1;
    }
    //here means that we creates socket and bind success
    printf("socket bind successfully\n");

    // now we have to listen
    if((status = listen(sock,BACKLOG)) != 0){
        perror("listen");
        fprintf(stderr,"failed to listen, error: %s\n",strerror(status));
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    printf("we are listening on port %s\n",PORT);
    return sock;
}
void handle_client(int client_fd){

    struct timeval timeout = {.tv_sec = SOCK_TIMEOUT, .tv_usec =0};
    // timeout.tv_sec = SOCK_TIMEOUT;
    // timeout.tv_usec = 0;
    ssize_t bytesTransferred;


    char msg[BUFF_SIZE];

    
    // do{
        if(setsockopt(client_fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) < 0){
            perror("setsockopt() : SO_RCVTIMEO");
            close(client_fd);
            exit(EXIT_FAILURE);
        }
        while (childRunning){
            bytesTransferred = recv(client_fd,msg,sizeof msg,0);
            if(bytesTransferred == 0){
                //client closed
                break;
            }
            if(bytesTransferred == -1){
                if(errno == EINTR) continue;
                if(errno == EAGAIN || errno == EWOULDBLOCK ) {
                    // timeout happened
                    printf("--client handler-- timeout reached without receiving data\n");
                    break;
                }
                perror("recv");
                break;
            }
            printf("the message length is %ld\n",strlen(msg));
            int res = response(client_fd,msg,bytesTransferred);
            if(res == CLIENT_CLOSE_MSG){// the client sent the close message
                close(client_fd);
                exit(0);
            }
            if(res == -1){
                fprintf(stderr,"request incomprehensive, reqlen or req invalid\n");
                continue;
            }
            //echo back what we received
            // int msgLen ;
            // msgLen = strlen(msg);
            // bytesTransferred = send_full(client_fd,msg,msgLen);
            // printf("--client handler--<%d> bytes sent : %ld , excpected to send : %d\n",getpid(),bytesTransferred,msgLen);
        }

        // printf("exited the starting handler now waiting for se elements\n");
        // client_fd = dequeu_connection(pending_queue);
        // printf("the client sock after dequeu is <%d>\n",client_fd);

    // }while(rewind_handler);
    
    fflush(stdout);
    close(client_fd);// when done the client connection is closed
    return ;
}

int server_should_run(){
    return running;
}
ssize_t recv_full(int socket_fd,char *buf,size_t buflen){
    size_t n = 0;
    do{
        // printf("went through iteration in reception\n");
        n = recv(socket_fd,(buf + n),(buflen - n) ,0);
        if ((int) n == -1){
            break;
        }
    } while (n < buflen);
    return buflen - n;
}
ssize_t send_full(int socket_fd,char *buf,size_t buflen){
    size_t n = 0;
    do{
        // printf("went through iteration\n");
        n = send(socket_fd,(buf + n),(buflen - n) ,0);
        if ((int) n == -1){
            break;
        }
    } while (n < buflen);
    return buflen - n;
}
// ssize_t recv_full(int fd, void *buf, size_t len) {
//     char *p = buf;
//     size_t remaining = len;
//     while (remaining > 0) {
//         ssize_t n = recv(fd, p, remaining, 0);
//         if (n == 0) {
//             // peer closed; return how much we got so far
//             break;
//         }
//         if (n == -1) {
//             if (errno == EINTR) continue; // retry
//             return -1; // error
//         }
//         p += n;
//         remaining -= n;
//     }
//     return len - remaining; // bytes actually received
// }

void format_client_addresse(struct sockaddr *ca, char *out, size_t outlen){

    // (void)outlen;
    if(ca->sa_family == AF_INET){
        char ip[INET_ADDRSTRLEN];
        struct sockaddr_in *sin = (struct sockaddr_in *) ca;
        if(inet_ntop(sin->sin_family,&sin->sin_addr,ip,sizeof ip) == NULL){
            printf("invalid ip address\n");
            return;
        }
        snprintf(out,outlen,"%s:%d",ip,ntohs(sin->sin_port));
        printf("--client address formatter-- address ipv4 : %s port :%d\n",ip,ntohs(sin->sin_port));
    }else if (ca->sa_family == AF_INET6){
        char ip[INET6_ADDRSTRLEN];
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) ca;
        if(inet_ntop(sin6->sin6_family,&sin6->sin6_addr,ip,sizeof ip) == NULL){
            printf("invalid ip address\n");
            return;
        }
        snprintf(out,outlen,"%s:%d",ip,ntohs(sin6->sin6_port));
        printf("--client address formatter-- address ipv6 : %s port :%d\n",ip,ntohs(sin6->sin6_port));
    }
}

void install_child_sig_handler(void){
    struct sigaction sa_int = {0};
    sa_int.sa_handler = handle_child_sig_int;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGTERM,&sa_int,NULL);
}
void handle_child_sig_int(int sig){
    (void)sig;
    childRunning = 0;
    rewind_handler = 0;
}
void cleanup_children(void){
    pid_t pid;
    while (children_pids != NULL){
        // print_processes(children_pids);
        pid = dequeu_pid(&children_pids);
        // printf("killing child <%d>\n",pid);
        kill(pid,SIGTERM);
        childrenCount--;
    }
    // optionally wait for them to exit
    //dunno about this one tho
    // for (int i = 0; i < childrenCount; i++) {
    //     waitpid(childrenCount[i], NULL, 0);
    // }
}

// this function return 1 when we get the cl
int response(int sock,char *request, ssize_t reqlen){
    if(request == NULL || reqlen == 0){
        char invalid_req[] = "invalid request\n";
        send_full(sock,invalid_req,strlen(invalid_req));
        return -1;
    }
    char buffer[BUFF_SIZE];
    size_t len = ((size_t) reqlen < sizeof(buffer) - 1) ? (size_t) reqlen : sizeof(buffer) - 1;
    memcpy(buffer,request,len);
    buffer[len] = '\0'; //null termination
    
    char *newline = strpbrk(buffer,"\r\n");
    if(newline) *newline = '\0';
    printf("received command -[%s]-\n",buffer);
    if(strcmp(buffer,"PING") == 0){
        send_full(sock,"PONG",4);
    }else if(strcmp(buffer,"HELLO") == 0){
        send_full(sock,"THERE",5);
    }else if(strcmp(buffer,"CLOSE") == 0){
        send_full(sock,"BYE",3);
        return CLIENT_CLOSE_MSG;
    }else if(strcmp(buffer,"TIME") == 0){
        time_t t = time(NULL);
        char timebuff[64];
        snprintf(timebuff,sizeof timebuff,"Server time is : %s\n",ctime(&t));
        send_full(sock,timebuff,strlen(timebuff));
    }else{
        send(sock,"Unknown command\n",16,0);
    }
    return 0;
}
static int send_file_descriptor(int socket,int fd_to_send){
 struct msghdr message;
 struct iovec iov[1];
 struct cmsghdr *control_message = NULL;
 char ctrl_buf[CMSG_SPACE(sizeof(int))];
 char data[1];

 memset(&message, 0, sizeof(struct msghdr));
 memset(ctrl_buf, 0, CMSG_SPACE(sizeof(int)));

 /* We are passing at least one byte of data so that recvmsg() will not return 0 */
 data[0] = ' ';
 iov[0].iov_base = data;
 iov[0].iov_len = sizeof(data);

 message.msg_name = NULL;
 message.msg_namelen = 0;
 message.msg_iov = iov;
 message.msg_iovlen = 1;
 message.msg_controllen =  CMSG_SPACE(sizeof(int));
 message.msg_control = ctrl_buf;

 control_message = CMSG_FIRSTHDR(&message);
 control_message->cmsg_level = SOL_SOCKET;
 control_message->cmsg_type = SCM_RIGHTS;
 control_message->cmsg_len = CMSG_LEN(sizeof(int));

 *((int *) CMSG_DATA(control_message)) = fd_to_send;

 return sendmsg(socket, &message, 0);
}
void child_main(int ipc_sock, int index){
    (void)index;
    install_child_sig_handler();
    while(childRunning){
        int newSocket = recv_file_descriptor(ipc_sock);
        printf("in child, received from the ipc sock -%d-\n",newSocket);
        if(newSocket >= 0){
            handle_client(newSocket);
            close(newSocket);
            char msg = 'R'; // ready message
            send(ipc_sock,msg,1,0);
        }
    }
}

void handleSigInt(int sig){
    // this function handles main process interruption
    (void)sig;
    running = 0;
}

void handleSigChild(int sig){
    (void)sig;
}

void add_child(pid_t pid){
    if (childrenCount <= MAX_CHILDREN){
        printf("children count %d\n",childrenCount);
        if(children_pids == NULL){
            children_pids = create_process_node(pid);
        }else {
            add_process(pid,children_pids);
        }
        childrenCount++;
    }
    else{
        perror("max number of children processes reached");
    }
}

void enqueue_connection(shared_queue_t *q, int fd){
    sem_wait(slots);
    sem_wait(mutex);
    q->queue[q->tail] = fd;
    q->tail = (q->tail + 1) % MAX_QUEUE_SIZE; // circular queue
    q->count += 1;
    sem_post(mutex);
    sem_post(items);
}

int dequeu_connection(shared_queue_t *q){
    int sock;
    sem_wait(items);
    sem_wait(mutex);
    sock = q->queue[q->head];
    q->head = (q->head + 1) % MAX_QUEUE_SIZE; // circular queue
    q->count -= 1;
    sem_post(mutex);
    sem_post(slots);
    return sock;
}
static int recv_file_descriptor(int socket) {
    struct msghdr message;
    struct iovec iov[1];
    struct cmsghdr *control_message = NULL;
    char ctrl_buf[CMSG_SPACE(sizeof(int))];
    char data[1];
    int received_fd = -1;

    memset(&message, 0, sizeof(struct msghdr));
    memset(ctrl_buf, 0, sizeof(ctrl_buf));

    /* Same trick — we expect at least 1 byte of data */
    iov[0].iov_base = data;
    iov[0].iov_len = sizeof(data);

    message.msg_name = NULL;
    message.msg_namelen = 0;
    message.msg_iov = iov;
    message.msg_iovlen = 1;
    message.msg_control = ctrl_buf;
    message.msg_controllen = sizeof(ctrl_buf);

    if (recvmsg(socket, &message, 0) < 0) {
        perror("recvmsg");
        return -1;
    }

    /* Walk the control messages looking for SCM_RIGHTS */
    for (control_message = CMSG_FIRSTHDR(&message);
         control_message != NULL;
         control_message = CMSG_NXTHDR(&message, control_message)) {
        
        if ((control_message->cmsg_level == SOL_SOCKET) &&
            (control_message->cmsg_type == SCM_RIGHTS)) {
            
            received_fd = *((int *) CMSG_DATA(control_message));
            break;
        }
    }

    return received_fd;
}
