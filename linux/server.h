#ifndef SERVER
#define SERVER

void install_sig_inter_handler(void);
void reap_children_non_block(void);
void reap_children_blocking(void);
void handle_client(int client_fd);
int setup_listener(char *port);

#endif