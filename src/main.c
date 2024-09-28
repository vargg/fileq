#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "args.h"
#include "processing.h"


u_int8_t major = 0;
u_int8_t minor = 1;
u_int8_t patch = 0;


char *get_current_varsion() {
    char *version = malloc(sizeof(char) * 12);
    sprintf(version, "%d.%d.%d", major, minor, patch);
    return version;
}


int main(int argc, char *argv[]) {
    int exit_code = make_dirs();

    if (exit_code != EXIT_SUCCESS) {
        return exit_code;
    }

    InputArgs *args = get_empty_command_args();
    exit_code = parse_args(args, argc, argv);

    if (exit_code != EXIT_SUCCESS) {
        return exit_code;
    }

    if (args->command == HELP) {
        char *version = get_current_varsion();
        print_help(argv[0], version);
        free(version);
    } else if (args->command == LIST) {
        print_available_queues();
    } else if (args->command == CLEAR) {
        exit_code = remove_queue(args->q_name);
        if (exit_code == EXIT_SUCCESS) {
            exit_code = make_q(args->q_name);
        }
    } else if (args->command == DEL) {
        exit_code = remove_queue(args->q_name);
    } else if (args->command == SIZE) {
        exit_code = print_size_of_queue(args->q_name);
    } else if (args->command == SHOW) {
        exit_code = print_all_messages_from_queue(args->q_name);
    } else if (args->command == PULL) {
        exit_code = pull_message_from_queue(args->q_name);
    } else if (args->command == PUSH) {
        exit_code = make_q(args->q_name);
        if (exit_code == EXIT_SUCCESS) {
            exit_code = push_message_into_queue(args->q_name, args->message);
        }
    } else {
        printf("Unknown command\n");
        return EXIT_FAILURE;
    }

    destroy_command_args_obj(args);

    return exit_code;
}
