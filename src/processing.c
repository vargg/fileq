#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


const char *work_dir_name = ".fileq";
const char *qs_subdir_name = "qs";
const u_int8_t batch_size = 64;
const u_int8_t max_num_of_skipped_items = 16;


typedef struct {
    u_int64_t head;  // position of the first message
    u_int64_t skipped;  // number of messages pulled but not deleted yet
    u_int64_t size;  // total number of valid messages
} FileMetadata;

const u_int8_t metadata_size = sizeof(FileMetadata);

typedef struct {
    FILE *file;
    FileMetadata *metadata;
} Queue;


// HELPERS

char *get_work_dir_path() {
    char *home_dir = getenv("HOME");

    char *full_dir_path = malloc(strlen(home_dir) + strlen(work_dir_name) + 2);
    strcpy(full_dir_path, home_dir);
    strcat(full_dir_path, "/");
    strcat(full_dir_path, work_dir_name);

    return full_dir_path;
}

char *get_qs_dir_path() {
    char *work_dir = get_work_dir_path();

    char *full_dir_path = malloc(strlen(work_dir) + strlen(qs_subdir_name) + 2);
    strcpy(full_dir_path, work_dir);
    strcat(full_dir_path, "/");
    strcat(full_dir_path, qs_subdir_name);

    free(work_dir);

    return full_dir_path;
}


char *get_q_path(char *q_name) {
    char *dir_path = get_qs_dir_path();

    char *q_path = malloc(strlen(dir_path) + strlen(q_name) + 2);
    strcpy(q_path, dir_path);
    strcat(q_path, "/");
    strcat(q_path, q_name);

    free(dir_path);

    return q_path;
}


int check_or_make_dir(char *path) {
    struct stat st = {0};

    int code = EXIT_SUCCESS;
    if (stat(path, &st) == -1) {
        code = mkdir(path, 0700);
    }

    return code;
}


int make_dirs() {
    int code = check_or_make_dir(get_work_dir_path());

    if (code == EXIT_SUCCESS) {
        code = check_or_make_dir(get_qs_dir_path());
    }

    return code;
}

// remove extracted messages from file head
void _trim(FILE *file, FileMetadata *metadata) {
    u_int64_t shift = metadata_size;

    char *message = malloc(batch_size);
    while (1) {
        fseek(file, metadata->head + shift - metadata_size, SEEK_SET);

        if (fgets(message, batch_size, file) == NULL) {
            break;
        }

        fseek(file, shift, SEEK_SET);
        fwrite(message, strlen(message), 1, file);

        shift += strlen(message);
    }

    free(message);

    ftruncate(fileno(file), shift);

    metadata->head = metadata_size;
    metadata->skipped = 0;
}

int write_metadata(FILE *file, FileMetadata *metadata) {
    if (fseek(file, 0, SEEK_SET) != 0) {
        printf("failed to seek\n");
        return EXIT_FAILURE;
    }

    u_int64_t array[3] = {metadata->head, metadata->skipped, metadata->size};

    int size = fwrite(array, sizeof(u_int64_t), 3, file);
    if (size != 3) {
        printf("failed to write metadata\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


FileMetadata *read_metadata(FILE *file) {
    if (fseek(file, 0, SEEK_SET) != 0) {
        printf("failed to seek\n");
        return NULL;
    }

    u_int64_t array[3];
    int size = fread(array, sizeof(u_int64_t), 3, file);
    if (size != 3) {
        printf("failed to read metadata\n");
        return NULL;
    }

    FileMetadata *metadata = (FileMetadata *) malloc(sizeof(FileMetadata));
    if (metadata == NULL) {
        printf("failed to allocate memory for metadata\n");
        return NULL;
    }

    metadata->head = array[0];
    metadata->skipped = array[1];
    metadata->size = array[2];

    return metadata;
}


int make_q(char *name) {
    char *path = get_q_path(name);

    if (access(path, F_OK) == 0) {
        free(path);
        return EXIT_SUCCESS;
    }

    FILE *file = fopen(path, "wb");
    FileMetadata metadata = {.head = metadata_size, .skipped = 0, .size = 0};
    int code = write_metadata(file, &metadata);

    fclose(file);
    free(path);

    return code;
}


Queue *open_q(char *name, char *mode) {
    char *path = get_q_path(name);

    FILE *file = fopen(path, mode);
    free(path);

    if (file == NULL) {
        printf("queue `%s` does not exist\n", name);
        return NULL;
    }

    FileMetadata *metadata = read_metadata(file);
    if (metadata == NULL) {
        printf("queue is broken\n");
        fclose(file);
        return NULL;
    }

    Queue *queue = (Queue *) malloc(sizeof(Queue));
    queue->file = file;
    queue->metadata = metadata;

    return queue;
}

void destroy_q_obj(Queue *queue) {
    fclose(queue->file);
    free(queue->metadata);
    free(queue);
}


// HANDLERS

void print_available_queues() {
    struct dirent *d;

    DIR *dir = opendir(get_qs_dir_path());
    if (dir) {
        while ((d = readdir(dir)) != NULL) {
            if(d->d_type == DT_REG){
                printf("%s\n", d->d_name);
            }
        }
        closedir(dir);
    }
}


int print_size_of_queue(char *q_name) {
    Queue *queue = open_q(q_name, "rb");

    if (queue == NULL) {
        return EXIT_FAILURE;
    }

    printf("%lu\n", queue->metadata->size);

    destroy_q_obj(queue);

    return EXIT_SUCCESS;
}


int remove_queue(char *q_name) {
    char *path = get_q_path(q_name);
    int exit_code = 0;

    if (remove(path) != 0) {
        printf("failed to delete queue `%s`\n", q_name);
        exit_code = EXIT_FAILURE;
    }
    free(path);

    return exit_code;
}


int print_all_messages_from_queue(char *q_name) {
    Queue *queue = open_q(q_name, "rb");

    if (queue == NULL) {
        return EXIT_FAILURE;
    }

    char *message = malloc(batch_size);
    if (fseek(queue->file, queue->metadata->head, SEEK_SET) != 0) {
        printf("failed to seek\n");
        return EXIT_FAILURE;
    }

    while (fgets(message, batch_size, queue->file) != NULL) {
        printf("%s", message);
    }

    free(message);

    destroy_q_obj(queue);
    return EXIT_SUCCESS;
}


int pull_message_from_queue(char *q_name) {
    Queue *queue = open_q(q_name, "rb+");

    if (queue == NULL) {
        return EXIT_FAILURE;
    }

    if (queue->metadata->size == 0) {
        printf("queue `%s` is empty\n", q_name);
        return EXIT_FAILURE;
    }

    char *message = malloc(batch_size);
    if (fseek(queue->file, queue->metadata->head, SEEK_SET) != 0) {
        printf("failed to seek\n");
        return EXIT_FAILURE;
    }

    int shift = 0;
    while (1) {
        fgets(message, batch_size, queue->file);
        printf("%s", message);
        shift += strlen(message);
        if (message[strlen(message) - 1] == '\n') {
            break;
        }
    }

    free(message);

    queue->metadata->head += shift;
    queue->metadata->skipped++;
    queue->metadata->size--;

    if (queue->metadata->skipped > max_num_of_skipped_items) {
        _trim(queue->file, queue->metadata);
    }

    int code = write_metadata(queue->file, queue->metadata);

    destroy_q_obj(queue);

    return code;
}


int push_message_into_queue(char *q_name, char *message) {
    Queue *queue = open_q(q_name, "rb+");

    if (queue == NULL) {
        return EXIT_FAILURE;
    }

    if (fseek(queue->file, 0, SEEK_END) != 0) {
        printf("failed to seek\n");
        return EXIT_FAILURE;
    }

    fputs(message, queue->file);
    fputs("\n", queue->file);

    queue->metadata->size++;

    int code = write_metadata(queue->file, queue->metadata);

    destroy_q_obj(queue);

    return code;
}
