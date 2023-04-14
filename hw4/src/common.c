#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "ticker.h"
#include "common.h"
#include "debug.h"

static int quiting;
static int cur_watcher_num;
static int max_watcher_num;
static COMMON_WATCHER **watcher_list;

void init_watcher() {
    max_watcher_num = 2;
    watcher_list = malloc(sizeof(COMMON_WATCHER *) * max_watcher_num);
    memset(watcher_list, 0, sizeof(COMMON_WATCHER *) * max_watcher_num);
}

COMMON_WATCHER *find_watcher_by_reading_fd(int fd) {
    int num = 0;
    for (int i = 0; i < max_watcher_num && num < cur_watcher_num; i++) {
        if (watcher_list[i] == NULL) {
            continue;
        }
        num++;
        if (watcher_list[i]->reading_fd == fd) {
            return watcher_list[i];
        }
    }

    return NULL;
}

void  grow_watcher() {
    watcher_list = realloc(watcher_list, sizeof(COMMON_WATCHER *) * max_watcher_num * 2);
    memset((char*)watcher_list + sizeof(COMMON_WATCHER *) * max_watcher_num, 0, 
        sizeof(COMMON_WATCHER *) * max_watcher_num);

    max_watcher_num *= 2;
}

COMMON_WATCHER *create_watcher(int pid, int reading_fd, int writing_fd) {
    COMMON_WATCHER *watcher = malloc(sizeof(COMMON_WATCHER));
    memset(watcher, 0, sizeof(COMMON_WATCHER));
    watcher->pid = pid;
    watcher->reading_fd = reading_fd;
    watcher->writing_fd = writing_fd;

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
    int num = 0;
    COMMON_WATCHER *watcher = NULL;
    for (int i = 0; i < max_watcher_num && num < cur_watcher_num; i++) {
        if (watcher_list[i] == NULL) {
            continue;
        }
        num++;

        if (watcher_list[i]->pid == pid) {
            watcher = watcher_list[i];
            watcher_list[i] = NULL;
            break;
        }
    }

    if (watcher == NULL) {
        return;
    }

    close(watcher->reading_fd);
    close(watcher->writing_fd);
    for (int i = 0; i < sizeof(watcher->args)/sizeof(char *); i++) {
        if (watcher->args[i] == NULL) {
            break;
        }
        free(watcher->args[i]);
    }
    if (watcher->partial_line != NULL) {
        free(watcher->partial_line);
    }
    free(watcher);
    cur_watcher_num--;
    return;
}

COMMON_WATCHER *get_watcher(int index) {
    if (index <= 0 || index >= max_watcher_num) {
        return NULL;
    }

    return watcher_list[index];
}

COMMON_WATCHER *find_watcher_by_pid(int pid) {
    int num = 0;
    for (int i = 0; i < max_watcher_num && num < cur_watcher_num; i++) {
        if (watcher_list[i] == NULL) {
            continue;
        }
        num++;
        if (watcher_list[i]->pid == pid) {
            return watcher_list[i];
        }
    }

    return NULL;
}

void print_watchers() {
    int num = 0;
    for (int i = 0; i < max_watcher_num && num < cur_watcher_num; i++) {
        if (watcher_list[i] == NULL) {
            continue;
        }
        num++;
        COMMON_WATCHER *watcher = watcher_list[i];
        WATCHER_TYPE *type = (WATCHER_TYPE *)watcher->watcher_type;
        printf("%d\t%s(%d,%d,%d)", i, type->name, watcher->pid,
            watcher->reading_fd,  watcher->writing_fd);
        
        if (type->argv == NULL) {
            printf("\n");
            continue;
        }

        for (int j = 0; type->argv[j]; j++) {
            printf(" %s", type->argv[j]);
        }
        for (int j = 0; j < sizeof(watcher->args)/sizeof(char *); j++) {
            if (watcher->args[j] == NULL) {
                break;
            }
            if (j == 0) {
                printf("[");
            } else {
                printf(" ");
            }
            printf("%s", watcher->args[j]);
        }
        printf("]\n");
    }
}

void stop_watcher(int index) {
    if (index <= 0 || index >= max_watcher_num) {
        return;
    }

    if (watcher_list[index] == NULL) {
        return;
    }
    WATCHER_TYPE *type = (WATCHER_TYPE *)watcher_list[index]->watcher_type;
    type->stop((WATCHER *)watcher_list[index]);
}

void stop_one_watcher() {
    int num = 0;
    for (int i = 0; i < max_watcher_num && num < cur_watcher_num; i++) {
        if (watcher_list[i] == NULL) {
            continue;
        }
        num++;
        
        COMMON_WATCHER *watcher = watcher_list[i];
        if (watcher->pid < 0) {
            continue;
        }
        WATCHER_TYPE *type = (WATCHER_TYPE *)watcher->watcher_type;
        type->stop((WATCHER *)watcher);
        break;
    }
}


FILE* create_memstream(COMMON_WATCHER *watcher, char *txt) {
    FILE *fp = open_memstream(&watcher->input_buf, &watcher->input_size);
    if (watcher->partial_line_size != 0) {
        fwrite(watcher->partial_line, 1, watcher->partial_line_size, fp);
    }
    fwrite(txt, 1, strlen(txt), fp);
    watcher->input = fp;
    return fp;
}

void close_memstream(COMMON_WATCHER *watcher, char *partial_line) {
    fclose(watcher->input);
    free(watcher->input_buf);
    watcher->input = NULL;
    watcher->input_buf = NULL;
    watcher->input_size = 0;
    watcher->partial_line_size = 0;
    if (partial_line == NULL) {
        return;
    }

    size_t size = strlen(partial_line);
    if (size > 0) {
        watcher->partial_line = realloc(watcher->partial_line, size);
        memcpy(watcher->partial_line, partial_line, size);
        watcher->partial_line_size = size;
    }
}

void quit() {
    quiting = 1;
    stop_one_watcher();
    if (cur_watcher_num == 1) {
        exit(0);
    }
}

int is_quit() {
    return quiting;
}

int get_watcher_num() {
    return cur_watcher_num;
}

