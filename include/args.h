
#ifndef args_h
#define args_h


// the order of items is important
typedef enum {
    // no extra args
    HELP,
    LIST,

    // with queue name
    PULL,
    SHOW,
    SIZE,

    // with queue name and message
    PUSH,
} CommandTypes;


typedef struct InputArgs {
    CommandTypes command;
    char *q_name;
    char *message;
} InputArgs;


void print_help(char *name, char *version);
int parse_args(InputArgs *args, int argc, char *argv[]);
InputArgs *get_empty_command_args();
void destroy_command_args_obj(InputArgs *args);

#endif
