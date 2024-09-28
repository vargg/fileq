
#ifndef processing_h
#define processing_h

int make_dirs();
int make_q(char *name);

void print_available_queues();
int remove_queue(char *q_name);
int print_size_of_queue(char *q_name);
int print_all_messages_from_queue(char *q_name);
int pull_message_from_queue(char *q_name);
int push_message_into_queue(char *q_name, char *message);

#endif
