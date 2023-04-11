
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

typedef struct common_watcher {
    void* watcher_type;
    pid_t pid;
    int reading_fd;
    int writing_fd;
    int stop;
    char *args[10];
    int enable_trace;
    int serial_number;
    FILE *input;
    char *input_buf;
    size_t input_size;
    char *partial_line;
    size_t partial_line_size;
} COMMON_WATCHER;

void init_watcher();
COMMON_WATCHER *find_watcher_by_reading_fd(int fd);
COMMON_WATCHER *create_watcher(int pid, int reading_fd, int writing_fd);
COMMON_WATCHER *get_watcher(int index);
void add_watcher(WATCHER *watcher);
void remove_watcher(int pid);
void print_watchers();
void stop_one_watcher();
void stop_watcher(int index);
void quit();
int is_quit();
int get_watcher_num();
FILE* create_memstream(COMMON_WATCHER *watcher, char *txt);
void close_memstream(COMMON_WATCHER *watcher, char *partial_line);
#endif
