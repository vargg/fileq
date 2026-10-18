#include "test.h"

// included directly so static helpers are reachable from the tests
#include "../src/args.c"


#define ARGV(...) ((char *[]) {"fq", __VA_ARGS__, NULL})

static InputArgs *args;
static int code;
static char *output;

// parse a NULL-terminated argv, keeping the result, exit code and printed output
static void parse(char *argv[]) {
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }

    args = get_empty_command_args();
    capture_begin();
    code = parse_args(args, argc, argv);
    output = capture_end();
}

static void done(void) {
    destroy_command_args_obj(args);
    free(output);
}


TEST(no_args) {
    char *argv[] = {"fq", NULL};
    parse(argv);
    CHECK_INT_EQ(code, EXIT_SUCCESS);
    CHECK_INT_EQ(args->command, HELP);
    done();
}

TEST(command_names) {
    parse(ARGV("help"));
    CHECK_INT_EQ(code, EXIT_SUCCESS);
    CHECK_INT_EQ(args->command, HELP);
    done();

    parse(ARGV("list"));
    CHECK_INT_EQ(code, EXIT_SUCCESS);
    CHECK_INT_EQ(args->command, LIST);
    CHECK(args->q_name == NULL);
    done();

    // commands that take only a queue name
    struct { char *name; CommandTypes command; } q_commands[] = {
        {"clear", CLEAR}, {"del", DEL}, {"pull", PULL}, {"show", SHOW}, {"size", SIZE},
    };
    for (size_t i = 0; i < sizeof(q_commands) / sizeof(q_commands[0]); i++) {
        parse(ARGV(q_commands[i].name, "-q", "t"));
        CHECK_INT_EQ(code, EXIT_SUCCESS);
        CHECK_INT_EQ(args->command, q_commands[i].command);
        CHECK_STR_EQ(args->q_name, "t");
        CHECK(args->message == NULL);
        done();
    }

    parse(ARGV("push", "-q", "t", "m"));
    CHECK_INT_EQ(code, EXIT_SUCCESS);
    CHECK_INT_EQ(args->command, PUSH);
    CHECK_STR_EQ(args->message, "m");
    done();
}

TEST(unknown_command) {
    parse(ARGV("pul", "-q", "t"));
    CHECK_INT_EQ(code, EXIT_FAILURE);
    CHECK(strstr(output, "unknown command `pul`") != NULL);
    done();
}

TEST(queue_name_forms) {
    char *forms[][2] = {{"-q", "name"}, {"-q=name", NULL}, {"-qname", NULL}};
    for (size_t i = 0; i < sizeof(forms) / sizeof(forms[0]); i++) {
        if (forms[i][1] != NULL) {
            parse(ARGV("size", forms[i][0], forms[i][1]));
        } else {
            parse(ARGV("size", forms[i][0]));
        }
        CHECK_INT_EQ(code, EXIT_SUCCESS);
        CHECK_STR_EQ(args->q_name, "name");
        done();
    }

    // the message starts right after the queue name in every form
    parse(ARGV("push", "-q=name", "msg"));
    CHECK_INT_EQ(code, EXIT_SUCCESS);
    CHECK_STR_EQ(args->q_name, "name");
    CHECK_STR_EQ(args->message, "msg");
    done();
}

TEST(queue_name_missing) {
    parse(ARGV("size"));
    CHECK_INT_EQ(code, EXIT_FAILURE);
    done();

    parse(ARGV("size", "-x", "t"));
    CHECK_INT_EQ(code, EXIT_FAILURE);
    done();

    parse(ARGV("size", "-q"));
    CHECK_INT_EQ(code, EXIT_FAILURE);
    done();

    parse(ARGV("size", "-q="));
    CHECK_INT_EQ(code, EXIT_FAILURE);
    done();
}

TEST(invalid_queue_names) {
    char *names[] = {"..", ".", ".hidden", "a/b", "../../x", "a b", ""};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        parse(ARGV("size", "-q", names[i]));
        if (code != EXIT_FAILURE) {
            fprintf(stderr, "    accepted invalid name \"%s\"\n", names[i]);
        }
        CHECK_INT_EQ(code, EXIT_FAILURE);
        CHECK(strstr(output, "invalid queue name") != NULL);
        done();
    }
}

TEST(valid_queue_names) {
    char *names[] = {"a", "A-z_0.9", "x.y", "trailing."};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        parse(ARGV("size", "-q", names[i]));
        CHECK_INT_EQ(code, EXIT_SUCCESS);
        CHECK_STR_EQ(args->q_name, names[i]);
        done();
    }
}

TEST(message_join) {
    parse(ARGV("push", "-q", "t", "hello", "world"));
    CHECK_INT_EQ(code, EXIT_SUCCESS);
    CHECK_STR_EQ(args->message, "hello world");
    done();

    // spacing inside a single argument is kept as is
    parse(ARGV("push", "-q", "t", "a  b", "c"));
    CHECK_INT_EQ(code, EXIT_SUCCESS);
    CHECK_STR_EQ(args->message, "a  b c");
    done();
}

TEST(message_missing) {
    parse(ARGV("push", "-q", "t"));
    CHECK_INT_EQ(code, EXIT_FAILURE);
    done();

    parse(ARGV("push", "-q", "t", ""));
    CHECK_INT_EQ(code, EXIT_FAILURE);
    done();
}

TEST(extra_words_ignored) {
    parse(ARGV("size", "-q", "t", "extra", "words"));
    CHECK_INT_EQ(code, EXIT_SUCCESS);
    CHECK_STR_EQ(args->q_name, "t");
    CHECK(args->message == NULL);
    done();
}


int main(int argc, char *argv[]) {
    test_init(argc, argv);

    RUN(no_args);
    RUN(command_names);
    RUN(unknown_command);
    RUN(queue_name_forms);
    RUN(queue_name_missing);
    RUN(invalid_queue_names);
    RUN(valid_queue_names);
    RUN(message_join);
    RUN(message_missing);
    RUN(extra_words_ignored);

    return test_report("test_args");
}
