
#ifndef args_h
#define args_h


typedef enum {
    HELP,
    LIST,
    SHOW,
    SIZE,
    PULL,
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
