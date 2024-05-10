#include <stdio.h>


const char RED[] = "\033[0;31m";
const char GREEN[] = "\033[0;32m";
const char NC[] = "\033[0m";


void log_error(const char message) {
    printf("%sError: %s%s\n", RED, message, NC);
}


void log_info(const char message) {
    printf("%s\n", message);
}
