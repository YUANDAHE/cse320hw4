#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "ticker.h"
#include "common.h"

static int cur_watcher_num;
static int max_watcher_num;
static COMMON_WATCHER **watcher_list;

void init_watcher() {
    max_watcher_num = 64;
    watcher_list = malloc(sizeof(COMMON_WATCHER *) * max_watcher_num);
    memset(watcher_list, 0, sizeof(COMMON_WATCHER *) * max_watcher_num);
}

COMMON_WATCHER *find_watcher_by_reading_fd(int fd) {
    for (int i = 0; i < cur_watcher_num;) {
        if (watcher_list[i] == NULL) {
            continue;
        }

        if (watcher_list[i]->reading_fd == fd) {
            return watcher_list[i];
        }
        i++;
    }

    return NULL;
}

void  grow_watcher() {
    watcher_list = realloc(watcher_list, sizeof(COMMON_WATCHER *) * max_watcher_num * 2);
    memset(watcher_list + sizeof(COMMON_WATCHER *) * max_watcher_num, 0, 
        sizeof(COMMON_WATCHER *) * max_watcher_num);

    max_watcher_num *= 2;
}

COMMON_WATCHER *create_watcher(int pid, int reading_fd, int writing_fd) {
    COMMON_WATCHER *watcher = malloc(sizeof(COMMON_WATCHER));
    watcher->pid = pid;
    watcher->reading_fd = reading_fd;
    watcher->writing_fd = writing_fd;
    watcher->input = open_memstream(&watcher->input_buf, &watcher->input_size);

    fcntl(reading_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
    fcntl(reading_fd, F_SETOWN, getpid());
    fcntl(reading_fd, F_SETSIG, SIGIO);

    return watcher;
}

void add_watcher(WATCHER *watcher) {
    int index = -1;
    for (int i = 0; i < max_watcher_num; i++) {
        if (watcher_list[i] == NULL) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        index = max_watcher_num;
        grow_watcher();
    }

    watcher_list[index] = (COMMON_WATCHER *)watcher;
    cur_watcher_num++;
}

void remove_watcher(int pid) {
    for (int i = 0; i < cur_watcher_num;) {
        if (watcher_list[i] == NULL) {
            continue;
        }

        if (watcher_list[i]->pid == pid) {
            fclose(watcher_list[i]->input);
            free(watcher_list[i]);
            watcher_list[i] = NULL;
            cur_watcher_num--;
            return;
        }
        i++;
    }
}

COMMON_WATCHER *find_watcher_by_pid(int pid) {
    for (int i = 0; i < cur_watcher_num;) {
        if (watcher_list[i] == NULL) {
            continue;
        }

        if (watcher_list[i]->pid == pid) {
            return watcher_list[i];
        }
        i++;
    }

    return NULL;
}

void print_watchers() {
    for (int i = 0; i < cur_watcher_num;) {
        if (watcher_list[i] == NULL) {
            continue;
        }
        COMMON_WATCHER *watcher = watcher_list[i];
        i++;

        WATCHER_TYPE *type = (WATCHER_TYPE *)watcher->watcher_type;

        printf("%d\t%s(%d,%d,%d)", i-1, type->name, watcher->pid,
            watcher->reading_fd,  watcher->writing_fd);
        
        if (type->argv == NULL) {
            printf("\n");
            continue;
        }
    }
}

void stop_all_watchers() {
    for (int i = 1; i < cur_watcher_num;) {
        if (watcher_list[i] == NULL) {
            continue;
        }
        COMMON_WATCHER *watcher = watcher_list[i];
        i++;

        if (watcher->pid < 0) {
            continue;
        }

        WATCHER_TYPE *type = (WATCHER_TYPE *)watcher->watcher_type;
        type->stop((WATCHER *)watcher);
    }
}

int get_watcher_num() {
    return cur_watcher_num;
}