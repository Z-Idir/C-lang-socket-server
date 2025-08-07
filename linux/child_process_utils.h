/*
---- meow ------
  So this here is the header for the child process management utils
  we define the linked list and functions to add, delete and search for a specific process
  happy reading
*/

#include <sys/types.h>
#ifndef CHILD_PROCESS_UTILS
#define CHILD_PROCESS_UTILS
typedef struct process_node {
    pid_t pid;
    struct process_node *next;
} pl ;//process list


void add_process(int pid, pl *head);
pl *create_process_node(pid_t pid);
void delete_node(pid_t pid,pl **head);
void free_list(pl **head);
pid_t dequeu_pid(pl **h);
void print_processes(pl *h);

#endif

