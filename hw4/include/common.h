
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

typedef struct common_watcher {
    void* watcher_type;
    pid_t pid;
    int reading_fd;
    int writing_fd;
    FILE *input;
    char *input_buf;
    size_t input_size;
} COMMON_WATCHER;

void init_watcher();
COMMON_WATCHER *find_watcher_by_reading_fd(int fd);
COMMON_WATCHER *create_watcher(int pid, int reading_fd, int writing_fd);
void add_watcher(WATCHER *watcher);
void remove_watcher(int pid);
void print_watchers();
void stop_all_watchers();
int get_watcher_num();
#endif
