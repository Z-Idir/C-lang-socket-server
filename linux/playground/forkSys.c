#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

void list_fds(const char *label) {
    char path[256];
    snprintf(path, sizeof path, "/proc/%d/fd", getpid());
    printf("[%s pid=%d] open fds: ", label, getpid());
    fflush(stdout);
    // list by reading the directory (simple: count entries)
    int count = 0;
    FILE *p = popen((char[]){0}, "r"); // dummy to silence unused
    (void)p;
    // instead of parsing, just show the symlinks via shell (for human inspection)
    char cmd[512];
    snprintf(cmd, sizeof cmd, "ls -l %s | awk '{print $9}'", path);
    system(cmd);
}

int main() {
    int sv[2]; // socketpair ends
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        perror("socketpair");
        exit(1);
    }

    printf("Initial: parent has both ends (sv[0]=%d, sv[1]=%d)\n", sv[0], sv[1]);
    list_fds("parent (before fork)");

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        // Child
        printf("\n-- child starts --\n");
        list_fds("child (after fork)");

        // child closes sv[1], keeps sv[0]; parent still has both
        close(sv[1]);
        printf("[child] closed sv[1], still has sv[0]\n");
        list_fds("child");

        // read a message from parent via sv[0]
        char buf[100];
        ssize_t n = read(sv[0], buf, sizeof buf - 1);
        if (n > 0) {
            buf[n] = 0;
            printf("[child] received from parent: %s\n", buf);
        }

        // send reply
        const char *reply = "pong";
        write(sv[0], reply, strlen(reply));
        printf("[child] sent reply and now exiting (closing sv[0])\n");
        close(sv[0]); // last reference in child
        exit(0);
    } else {
        // Parent
        printf("\n-- parent after fork --\n");
        list_fds("parent");

        // parent closes sv[0], keeps sv[1]; child has sv[0]
        close(sv[0]);
        printf("[parent] closed sv[0], still has sv[1]\n");
        list_fds("parent");

        // send message to child via sv[1]
        const char *msg = "ping";
        write(sv[1], msg, strlen(msg));
        printf("[parent] sent: %s\n", msg);

        // read reply from child
        char buf[100];
        ssize_t n = read(sv[1], buf, sizeof buf - 1);
        if (n > 0) {
            buf[n] = 0;
            printf("[parent] received reply: %s\n", buf);
        }

        // wait for child to exit
        int status;
        waitpid(pid, &status, 0);
        printf("[parent] child exited\n");

        // now parent closes last descriptor
        close(sv[1]);
        printf("[parent] closed sv[1], all descriptors gone\n");
        list_fds("parent (final)");
    }

    return 0;
}
