#ifndef test_h
#define test_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// a test is `TEST(name) { ... }`, run with `RUN(name);` from main();
// passing a test name as the first argument runs only that test

static const char *test_filter = NULL;
static int test_failed = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)

#define RUN(name) do { \
    if (test_filter == NULL || strcmp(test_filter, #name) == 0) { \
        test_failed = 0; \
        test_##name(); \
        if (test_failed) { \
            tests_failed++; \
            printf("FAIL %s\n", #name); \
        } else { \
            tests_passed++; \
            printf("ok   %s\n", #name); \
        } \
    } \
} while (0)

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "    %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
        test_failed = 1; \
    } \
} while (0)

#define CHECK_INT_EQ(a, b) do { \
    long long _a = (long long) (a), _b = (long long) (b); \
    if (_a != _b) { \
        fprintf(stderr, "    %s:%d: %s == %s failed: %lld != %lld\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        test_failed = 1; \
    } \
} while (0)

#define CHECK_STR_EQ(a, b) do { \
    const char *_a = (a), *_b = (b); \
    if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0) { \
        fprintf(stderr, "    %s:%d: %s == %s failed: \"%s\" != \"%s\"\n", \
                __FILE__, __LINE__, #a, #b, _a ? _a : "(null)", _b ? _b : "(null)"); \
        test_failed = 1; \
    } \
} while (0)


static inline void test_init(int argc, char *argv[]) {
    test_filter = (argc > 1) ? argv[1] : NULL;
}

static inline int test_report(const char *name) {
    printf("%s: %d passed, %d failed\n", name, tests_passed, tests_failed);
    if (test_filter != NULL && tests_passed + tests_failed == 0) {
        fprintf(stderr, "no test named `%s`\n", test_filter);
        return EXIT_FAILURE;
    }
    return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}


// HELPERS

// read the rest of a stream into a NUL-terminated buffer
static inline char *read_stream(FILE *file, size_t *len) {
    size_t size = 0, capacity = 256;
    char *data = malloc(capacity);
    size_t n;
    while ((n = fread(data + size, 1, capacity - size - 1, file)) > 0) {
        size += n;
        if (capacity - size - 1 == 0) {
            capacity *= 2;
            data = realloc(data, capacity);
        }
    }
    data[size] = '\0';
    if (len != NULL) {
        *len = size;
    }
    return data;
}

static inline char *read_file(const char *path, size_t *len) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }
    char *data = read_stream(file, len);
    fclose(file);
    return data;
}

// stdout capture, for code that reports results with printf
static FILE *capture_file = NULL;
static int capture_saved_fd = -1;

static inline void capture_begin(void) {
    fflush(stdout);
    capture_file = tmpfile();
    capture_saved_fd = dup(STDOUT_FILENO);
    dup2(fileno(capture_file), STDOUT_FILENO);
}

static inline char *capture_end(void) {
    fflush(stdout);
    dup2(capture_saved_fd, STDOUT_FILENO);
    close(capture_saved_fd);

    rewind(capture_file);
    char *output = read_stream(capture_file, NULL);
    fclose(capture_file);
    return output;
}

// point HOME at a new empty directory so tests never touch the real ~/.fileq;
// the directories are left in TMPDIR for the OS to clean up
static inline const char *use_fresh_home(void) {
    static char dir[1024];
    const char *tmp = getenv("TMPDIR");
    snprintf(dir, sizeof(dir), "%s/fileq-test.XXXXXX", (tmp && *tmp) ? tmp : "/tmp");
    if (mkdtemp(dir) == NULL) {
        perror("mkdtemp");
        exit(EXIT_FAILURE);
    }
    setenv("HOME", dir, 1);
    return dir;
}

#endif
