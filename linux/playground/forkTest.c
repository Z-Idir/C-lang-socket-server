#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(){
    pid_t pid = fork();
    if(pid < 0){ //error
        perror("fork failed");
        return 1;
    }else if(pid == 0){ //child
        printf("[child] pid = %d, parent = %d\n",getpid(),getppid());
        sleep(2);
        printf("[child] exiting...\n");
        exit(42);
    }else{ //parent
        int status;
        printf("[parent] spawned child pid = %d\n",pid);
        printf("parent will now wait for the child ...\n");
        pid_t w = waitpid(pid,&status,0); // this a blocking wait!!!!
        if(w == -1){
            perror("wait error");
            return 1;
        }
        printf("[parent] : waited for child (good parent)\n");
        if(WIFEXITED(status)){
            printf("[parent] : child process %d exited normally with code : %d\n",w,WEXITSTATUS(status));
        }else if(WIFSIGNALED(status)){
            printf("[parent] : child process %d killed with signal : %d\n",w,WTERMSIG(status));
        }
        exit(42);
    }
    return 0;
}