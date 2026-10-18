#include <signal.h>
#include <sys/wait.h>

#include "test.h"

// included directly so static helpers are reachable from the tests
#include "../src/processing.c"


#define WORKERS 4
#define PER_WORKER 200


static const char *home;

static void fresh_home(void) {
    home = use_fresh_home();
    CHECK_INT_EQ(make_dirs(), EXIT_SUCCESS);
}

// same steps as main() for `fq push`
static int push(char *q_name, char *message) {
    int code = make_q(q_name);
    if (code == EXIT_SUCCESS) {
        code = push_message_into_queue(q_name, message);
    }
    return code;
}

static char *pull(char *q_name, int *code) {
    capture_begin();
    int c = pull_message_from_queue(q_name);
    char *output = capture_end();
    if (code != NULL) {
        *code = c;
    }
    return output;
}

static char *size_of(char *q_name) {
    capture_begin();
    print_size_of_queue(q_name);
    return capture_end();
}

static char *show(char *q_name) {
    capture_begin();
    print_all_messages_from_queue(q_name);
    return capture_end();
}

static void read_header(char *q_name, u_int64_t header[3]) {
    char *path = get_q_path(q_name);
    size_t len = 0;
    char *data = read_file(path, &len);
    free(path);

    memset(header, 0, 3 * sizeof(u_int64_t));
    if (data != NULL && len >= 3 * sizeof(u_int64_t)) {
        memcpy(header, data, 3 * sizeof(u_int64_t));
    }
    free(data);
}

static long long file_size(char *q_name) {
    char *path = get_q_path(q_name);
    struct stat st;
    int code = stat(path, &st);
    free(path);
    return (code == 0) ? (long long) st.st_size : -1;
}

#define CHECK_PULL(q_name, expected) do { \
    int _code; \
    char *_out = pull(q_name, &_code); \
    CHECK_INT_EQ(_code, EXIT_SUCCESS); \
    CHECK_STR_EQ(_out, expected); \
    free(_out); \
} while (0)

#define CHECK_SIZE(q_name, expected) do { \
    char *_out = size_of(q_name); \
    CHECK_STR_EQ(_out, expected); \
    free(_out); \
} while (0)


// DIRECTORIES AND QUEUE CREATION

TEST(make_dirs_creates_qs) {
    fresh_home();

    char path[1100];
    snprintf(path, sizeof(path), "%s/.fileq/qs", home);
    struct stat st;
    CHECK(stat(path, &st) == 0 && S_ISDIR(st.st_mode));

    CHECK_INT_EQ(make_dirs(), EXIT_SUCCESS);
}

TEST(home_unset) {
    unsetenv("HOME");
    capture_begin();
    int code = make_dirs();
    char *output = capture_end();

    CHECK_INT_EQ(code, EXIT_FAILURE);
    CHECK(strstr(output, "HOME is not set") != NULL);
    free(output);
}

TEST(make_q_new) {
    fresh_home();
    CHECK_INT_EQ(make_q("a"), EXIT_SUCCESS);
    CHECK_INT_EQ(file_size("a"), 24);

    u_int64_t header[3];
    read_header("a", header);
    CHECK_INT_EQ(header[0], 24);
    CHECK_INT_EQ(header[1], 0);
    CHECK_INT_EQ(header[2], 0);
}

TEST(make_q_existing) {
    fresh_home();
    CHECK_INT_EQ(push("a", "x"), EXIT_SUCCESS);
    CHECK_INT_EQ(make_q("a"), EXIT_SUCCESS);
    CHECK_SIZE("a", "1\n");
    CHECK_PULL("a", "x\n");
}

TEST(make_q_no_temp_files) {
    fresh_home();
    CHECK_INT_EQ(make_q("a"), EXIT_SUCCESS);
    CHECK_INT_EQ(make_q("b"), EXIT_SUCCESS);

    char *qs_dir = get_qs_dir_path();
    DIR *dir = opendir(qs_dir);
    free(qs_dir);
    CHECK(dir != NULL);

    int entries = 0;
    struct dirent *d;
    while (dir != NULL && (d = readdir(dir)) != NULL) {
        if (strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0) {
            CHECK(d->d_name[0] != '.');
            entries++;
        }
    }
    if (dir != NULL) {
        closedir(dir);
    }
    CHECK_INT_EQ(entries, 2);
}


// PUSH, PULL AND SIZE

TEST(fifo_order) {
    fresh_home();
    CHECK_INT_EQ(push("q", "a"), EXIT_SUCCESS);
    CHECK_INT_EQ(push("q", "b"), EXIT_SUCCESS);
    CHECK_INT_EQ(push("q", "c"), EXIT_SUCCESS);
    CHECK_SIZE("q", "3\n");

    CHECK_PULL("q", "a\n");
    CHECK_PULL("q", "b\n");
    CHECK_PULL("q", "c\n");
    CHECK_SIZE("q", "0\n");
}

TEST(pull_empty) {
    fresh_home();
    CHECK_INT_EQ(make_q("e"), EXIT_SUCCESS);

    int code;
    char *output = pull("e", &code);
    CHECK_INT_EQ(code, EXIT_FAILURE);
    CHECK(strstr(output, "is empty") != NULL);
    free(output);
}

TEST(pull_missing) {
    fresh_home();

    int code;
    char *output = pull("nope", &code);
    CHECK_INT_EQ(code, EXIT_FAILURE);
    CHECK(strstr(output, "does not exist") != NULL);
    free(output);
}

TEST(show_does_not_consume) {
    fresh_home();
    CHECK_INT_EQ(push("q", "a"), EXIT_SUCCESS);
    CHECK_INT_EQ(push("q", "b"), EXIT_SUCCESS);

    char *output = show("q");
    CHECK_STR_EQ(output, "a\nb\n");
    free(output);

    CHECK_SIZE("q", "2\n");
    CHECK_PULL("q", "a\n");
}

TEST(list_skips_dot_files) {
    fresh_home();
    CHECK_INT_EQ(make_q("a"), EXIT_SUCCESS);
    CHECK_INT_EQ(make_q("b"), EXIT_SUCCESS);

    char *tmp_path = get_q_path(".a.123");
    FILE *tmp = fopen(tmp_path, "w");
    CHECK(tmp != NULL);
    if (tmp != NULL) {
        fclose(tmp);
    }
    free(tmp_path);

    capture_begin();
    print_available_queues();
    char *output = capture_end();

    // readdir order is unspecified, so check membership only
    CHECK(strncmp(output, "a\n", 2) == 0 || strstr(output, "\na\n") != NULL);
    CHECK(strncmp(output, "b\n", 2) == 0 || strstr(output, "\nb\n") != NULL);
    CHECK(strstr(output, ".a.123") == NULL);
    CHECK_INT_EQ(strlen(output), 4);
    free(output);
}


// ESCAPING

static char *escaped(const char *message) {
    FILE *file = tmpfile();
    _write_escaped(file, message);
    rewind(file);
    char *data = read_stream(file, NULL);
    fclose(file);
    return data;
}

static char *unescaped(const char *line) {
    capture_begin();
    _print_unescaped(line, strlen(line));
    return capture_end();
}

#define CHECK_ESCAPED(message, expected) do { \
    char *_out = escaped(message); \
    CHECK_STR_EQ(_out, expected); \
    free(_out); \
} while (0)

#define CHECK_UNESCAPED(line, expected) do { \
    char *_out = unescaped(line); \
    CHECK_STR_EQ(_out, expected); \
    free(_out); \
} while (0)

TEST(write_escaped) {
    CHECK_ESCAPED("plain", "plain");
    CHECK_ESCAPED("a\nb", "a\\nb");
    CHECK_ESCAPED("c\\d", "c\\\\d");
    CHECK_ESCAPED("x\\", "x\\\\");
    CHECK_ESCAPED("e\\nf", "e\\\\nf");
}

TEST(print_unescaped) {
    CHECK_UNESCAPED("plain", "plain");
    CHECK_UNESCAPED("a\\nb", "a\nb");
    CHECK_UNESCAPED("c\\\\d", "c\\d");
    CHECK_UNESCAPED("x\\\\", "x\\");
    CHECK_UNESCAPED("e\\\\nf", "e\\nf");

    // unknown escapes and a lone trailing backslash pass through
    CHECK_UNESCAPED("a\\tb", "a\\tb");
    CHECK_UNESCAPED("x\\", "x\\");
}

TEST(round_trip) {
    fresh_home();

    char long_message[201];
    memset(long_message, 'x', 200);
    long_message[200] = '\0';

    char *messages[] = {"line1\nline2", "back\\slash", "e\\nf", "trailing\\", "\n", long_message};
    size_t count = sizeof(messages) / sizeof(messages[0]);

    for (size_t i = 0; i < count; i++) {
        CHECK_INT_EQ(push("r", messages[i]), EXIT_SUCCESS);
    }

    for (size_t i = 0; i < count; i++) {
        char expected[256];
        snprintf(expected, sizeof(expected), "%s\n", messages[i]);
        CHECK_PULL("r", expected);
    }
    CHECK_SIZE("r", "0\n");
}


// TRIM, CLEAR, DELETE AND DAMAGED FILES

TEST(trim_keeps_order) {
    fresh_home();
    char message[16];
    for (int i = 1; i <= 40; i++) {
        snprintf(message, sizeof(message), "m%d", i);
        CHECK_INT_EQ(push("t", message), EXIT_SUCCESS);
    }
    long long full_size = file_size("t");

    for (int i = 1; i <= 40; i++) {
        char expected[16];
        snprintf(expected, sizeof(expected), "m%d\n", i);
        CHECK_PULL("t", expected);

        // the first trim happens once more than 16 messages were skipped
        if (i == max_num_of_skipped_items + 1) {
            u_int64_t header[3];
            read_header("t", header);
            CHECK_INT_EQ(header[0], 24);
            CHECK_INT_EQ(header[1], 0);
            CHECK_INT_EQ(header[2], 40 - i);

            long long remaining = 0;
            for (int j = i + 1; j <= 40; j++) {
                remaining += snprintf(NULL, 0, "m%d\n", j);
            }
            CHECK_INT_EQ(file_size("t"), 24 + remaining);
            CHECK(file_size("t") < full_size);
        }
    }
    CHECK_SIZE("t", "0\n");
}

TEST(interleaved_trim) {
    fresh_home();
    char message[16], expected[16];
    int next_pull = 1;

    for (int i = 1; i <= 30; i++) {
        snprintf(message, sizeof(message), "v%d", i);
        CHECK_INT_EQ(push("i", message), EXIT_SUCCESS);
    }
    for (int i = 31; i <= 60; i++) {
        snprintf(message, sizeof(message), "v%d", i);
        CHECK_INT_EQ(push("i", message), EXIT_SUCCESS);
        snprintf(expected, sizeof(expected), "v%d\n", next_pull++);
        CHECK_PULL("i", expected);
    }
    while (next_pull <= 60) {
        snprintf(expected, sizeof(expected), "v%d\n", next_pull++);
        CHECK_PULL("i", expected);
    }
    CHECK_SIZE("i", "0\n");
}

TEST(clear) {
    fresh_home();
    CHECK_INT_EQ(push("c", "a"), EXIT_SUCCESS);
    CHECK_INT_EQ(push("c", "b"), EXIT_SUCCESS);
    CHECK_PULL("c", "a\n");

    CHECK_INT_EQ(clear_queue("c"), EXIT_SUCCESS);
    CHECK_INT_EQ(file_size("c"), 24);

    u_int64_t header[3];
    read_header("c", header);
    CHECK_INT_EQ(header[0], 24);
    CHECK_INT_EQ(header[1], 0);
    CHECK_INT_EQ(header[2], 0);

    CHECK_INT_EQ(push("c", "z"), EXIT_SUCCESS);
    CHECK_PULL("c", "z\n");

    capture_begin();
    int code = clear_queue("missing");
    char *output = capture_end();
    CHECK_INT_EQ(code, EXIT_FAILURE);
    CHECK(strstr(output, "does not exist") != NULL);
    free(output);
}

TEST(remove) {
    fresh_home();
    CHECK_INT_EQ(make_q("r"), EXIT_SUCCESS);
    CHECK_INT_EQ(remove_queue("r"), EXIT_SUCCESS);
    CHECK_INT_EQ(file_size("r"), -1);

    capture_begin();
    int code = remove_queue("r");
    free(capture_end());
    CHECK_INT_EQ(code, EXIT_FAILURE);
}

TEST(broken_file) {
    fresh_home();
    CHECK_INT_EQ(push("b", "one"), EXIT_SUCCESS);

    // append an unterminated message and claim it is a full one
    char *path = get_q_path("b");
    FILE *file = fopen(path, "rb+");
    free(path);
    CHECK(file != NULL);
    if (file == NULL) {
        return;
    }
    fseek(file, 0, SEEK_END);
    fputs("two", file);
    u_int64_t size = 2;
    fseek(file, 2 * sizeof(u_int64_t), SEEK_SET);
    fwrite(&size, sizeof(size), 1, file);
    fclose(file);

    // a hang (the old behavior) kills the run instead of blocking it
    alarm(5);
    CHECK_PULL("b", "one\n");

    int code;
    char *output = pull("b", &code);
    alarm(0);

    CHECK_INT_EQ(code, EXIT_FAILURE);
    CHECK(strstr(output, "is broken") != NULL);
    free(output);
}


// CONCURRENCY

// returns the worker id of a "p<id>-<n>" line and stores n, or -1
static int parse_worker_line(const char *line, int *n) {
    int worker;
    if (sscanf(line, "p%d-%d", &worker, n) != 2 || worker < 0 || worker >= WORKERS) {
        return -1;
    }
    return worker;
}

static void wait_for_workers(pid_t pids[WORKERS]) {
    for (int w = 0; w < WORKERS; w++) {
        int status;
        CHECK(waitpid(pids[w], &status, 0) == pids[w]);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        if (WIFSIGNALED(status)) {
            fprintf(stderr, "    worker %d killed by signal %d\n", w, WTERMSIG(status));
        }
    }
}

TEST(concurrent_push_pull) {
    fresh_home();
    pid_t pids[WORKERS];

    // the queue does not exist yet, so the workers also race to create it
    for (int w = 0; w < WORKERS; w++) {
        pids[w] = fork();
        if (pids[w] == 0) {
            alarm(30);  // a hung worker is killed and fails the test
            int failed = 0;
            char message[32];
            for (int i = 1; i <= PER_WORKER; i++) {
                snprintf(message, sizeof(message), "p%d-%d", w, i);
                failed |= push("c", message) != EXIT_SUCCESS;
            }
            _exit(failed);
        }
    }
    wait_for_workers(pids);

    char expected_size[16];
    snprintf(expected_size, sizeof(expected_size), "%d\n", WORKERS * PER_WORKER);
    CHECK_SIZE("c", expected_size);

    // every message is stored exactly once, each worker's in push order
    char *output = show("c");
    int last[WORKERS] = {0};
    int lines = 0;
    for (char *line = strtok(output, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        int n;
        int w = parse_worker_line(line, &n);
        CHECK(w >= 0);
        if (w >= 0) {
            CHECK_INT_EQ(n, last[w] + 1);
            last[w] = n;
        }
        lines++;
    }
    free(output);
    CHECK_INT_EQ(lines, WORKERS * PER_WORKER);

    // concurrent pullers drain the queue, each message going to exactly one
    char out_paths[WORKERS][1100];
    for (int w = 0; w < WORKERS; w++) {
        snprintf(out_paths[w], sizeof(out_paths[w]), "%s/pulled.%d", home, w);
        pids[w] = fork();
        if (pids[w] == 0) {
            alarm(30);  // a hung worker is killed and fails the test
            if (freopen(out_paths[w], "w", stdout) == NULL) {
                _exit(EXIT_FAILURE);
            }
            int failed = 0;
            for (int i = 0; i < PER_WORKER; i++) {
                failed |= pull_message_from_queue("c") != EXIT_SUCCESS;
            }
            fflush(stdout);
            _exit(failed);
        }
    }
    wait_for_workers(pids);

    static char seen[WORKERS][PER_WORKER + 1];
    memset(seen, 0, sizeof(seen));
    int pulled = 0;
    for (int p = 0; p < WORKERS; p++) {
        char *data = read_file(out_paths[p], NULL);
        CHECK(data != NULL);
        if (data == NULL) {
            continue;
        }

        int puller_last[WORKERS] = {0};
        for (char *line = strtok(data, "\n"); line != NULL; line = strtok(NULL, "\n")) {
            int n;
            int w = parse_worker_line(line, &n);
            CHECK(w >= 0 && n >= 1 && n <= PER_WORKER);
            if (w >= 0 && n >= 1 && n <= PER_WORKER) {
                CHECK(!seen[w][n]);
                seen[w][n] = 1;
                // a single puller sees each worker's messages in order
                CHECK(n > puller_last[w]);
                puller_last[w] = n;
            }
            pulled++;
        }
        free(data);
    }
    CHECK_INT_EQ(pulled, WORKERS * PER_WORKER);
    CHECK_SIZE("c", "0\n");
}


int main(int argc, char *argv[]) {
    test_init(argc, argv);

    RUN(make_dirs_creates_qs);
    RUN(home_unset);
    RUN(make_q_new);
    RUN(make_q_existing);
    RUN(make_q_no_temp_files);
    RUN(fifo_order);
    RUN(pull_empty);
    RUN(pull_missing);
    RUN(show_does_not_consume);
    RUN(list_skips_dot_files);
    RUN(write_escaped);
    RUN(print_unescaped);
    RUN(round_trip);
    RUN(trim_keeps_order);
    RUN(interleaved_trim);
    RUN(clear);
    RUN(remove);
    RUN(broken_file);
    RUN(concurrent_push_pull);

    return test_report("test_processing");
}
