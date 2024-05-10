#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "args.h"


InputArgs *get_empty_command_args() {
    InputArgs *args = malloc(sizeof(InputArgs));

    if (args == NULL) {
        printf("InputArgs initialization failed\n");
        exit(EXIT_FAILURE);
    }

    args->command = HELP;
    args->q_name = NULL;
    args->message = NULL;
    return args;
}


void destroy_command_args_obj(InputArgs *args) {
    free(args->q_name);
    free(args);
}


void print_help(char *name, char *version) {
    printf("fileq v%s\n", version);
    printf("Usage: %s <command> [-q <queue_name> [<message>]]\n", name);
    printf("\n");
    printf("Available commands:\n");
    printf("    help\n");
    printf("        show this message and exit\n");
    printf("    list\n");
    printf("        show the names of available queues\n");
    printf("    show -q <queue_name>\n");
    printf("        show content (all messages) of the queue\n");
    printf("        messages will not be removed from the queue\n");
    printf("    size -q <queue_name>\n");
    printf("        show the size (number of messages) of the queue\n");
    printf("    pull -q <queue_name>\n");
    printf("        get the first (oldest) message from the queue\n");
    printf("    push -q <queue_name> <message>\n");
    printf("        add a new message to the queue\n");
}


u_int8_t _parse_command(char *command) {
    u_int8_t answer = HELP;
    if (strcmp(command, "list") == 0) {
        answer = LIST;
    } else if (strcmp(command, "show") == 0) {
        answer = SHOW;
    } else if (strcmp(command, "size") == 0) {
        answer = SIZE;
    } else if (strcmp(command, "pull") == 0) {
        answer = PULL;
    } else if (strcmp(command, "push") == 0) {
        answer = PUSH;
    }
    return answer;
}


u_int8_t _parse_queue_name(InputArgs *input_args, int argc, char *argv[]) {
    if (argc < 3) {
        printf("ERROR: -q param does not specified\n");
        return EXIT_FAILURE;
    }

    char *q_arg = argv[2];

    if (strlen(q_arg) < 2 || (q_arg[0] != '-' || q_arg[1] != 'q')) {
        printf("ERROR: -q param is incorrect\n");
        return EXIT_FAILURE;
    }

    u_int8_t q_name_index = (strcmp(q_arg, "-q") == 0) ? 3 : 2;
    if (
        (q_name_index == 2 && q_arg[2] == '=' && strlen(q_arg) < 4)
        || (q_name_index == 3 && argc < 4)
    ) {
        printf("ERROR: q name does not specified 1\n");
        return EXIT_FAILURE;
    }

    int q_name_len;
    u_int8_t prefix_size = 0;

    if (q_name_index == 3) {
        q_arg = argv[3];
        q_name_len = strlen(q_arg);
    } else {
        prefix_size = (q_arg[2] == '=') ? 3 : 2;  // -q=<name> or -q<name>
        q_name_len = strlen(q_arg) - prefix_size;
    }

    input_args->q_name = malloc(q_name_len + 1);
    strncat(input_args->q_name, q_arg + prefix_size, q_name_len);

    return EXIT_SUCCESS;
}


u_int8_t _parse_message(
    InputArgs *input_args,
    int argc,
    char *argv[],
    u_int8_t message_start_index
) {
    if (argc < message_start_index + 1 || strlen(argv[message_start_index]) < 1) {
        printf("ERROR: message does not specified\n");
        return EXIT_FAILURE;
    }

    u_int64_t message_size = strlen(argv[message_start_index]);

    for (u_int32_t i = message_start_index + 1; i < argc; i++) {
        message_size += strlen(argv[i]) + 1;  // +1 for space between messages
    }

    input_args->message = malloc(message_size + 1);

    for (u_int32_t i = message_start_index; i < argc; i++) {
        strcat(input_args->message, argv[i]);

        if (i < argc - 1) {
            strcat(input_args->message, " ");
        }
    }

    return EXIT_SUCCESS;
}


int parse_args(InputArgs *input_args, int argc, char *argv[]) {
    if (argc < 2) {
        input_args->command = HELP;
        return EXIT_SUCCESS;
    }

    input_args->command = _parse_command(argv[1]);

    if (input_args->command <= LIST) {
        return EXIT_SUCCESS;
    }

    if (_parse_queue_name(input_args, argc, argv) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    if (input_args->command < PUSH) {
        return EXIT_SUCCESS;
    }

    u_int8_t q_name_index = (strcmp(argv[2], "-q") == 0) ? 3 : 2;
    return _parse_message(input_args, argc, argv, q_name_index + 1);
}
