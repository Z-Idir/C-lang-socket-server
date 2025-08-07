#include <stdlib.h>
#include <stdio.h>
#include "child_process_utils.h"


// test dependencies
#include <unistd.h>

pl *create_process_node(pid_t pid){
    pl *h ;
    h = (pl *)malloc(sizeof (pl));
    h->next = NULL;
    h->pid = pid;
    return h;
}

void add_process(int pid, pl *head){
    if(head == NULL){
        fprintf(stderr,"head is NULL\n");
        return;
    }
    pl *n = create_process_node(pid);
    pl *iter ;
    for (iter = head; iter != NULL; iter = iter->next){
        if(iter->next == NULL){//we reached the tail
            iter->next = n;
            return;
        }
    }
    
}
void print_processes(pl *h){
    for (pl *i = h; i != NULL; i = i->next){
        printf("<%d>\n",i->pid);
    }
}

// this function deletes the first occurence of the pid, i will make improvements laturr
void delete_node(pid_t pid,pl **head){
    pl *prev = NULL;
    for(pl *i = *head; i != NULL; i = i->next){
        // printf("iter\n");
        if(i->pid == pid){
            if(prev ==NULL){
                prev = *head;
                *head = i->next;
                free(prev);
                return;
            }else{
                pl *temp = i;
                prev->next = i->next;
                free(temp);
                return;
            }
        }else {
            prev = i;
        }
    }
}
void traverse_list(pl *node){
    if(node == NULL){
        return;
    }
    traverse_list(node->next);
    free(node);
}
void free_list(pl **head){
    traverse_list(*head);
    *head = NULL;
}
// this function will be used to dequeu process pids without regard to the nodes themselves
pid_t dequeu_pid(pl **h){
    if(*h == NULL){
        return -1;
    }
    pl *temp = *h;
    *h = (*h)->next;
    pid_t p = temp->pid;
    free(temp);
    return p;
}
 