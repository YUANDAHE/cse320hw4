#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "ticker.h"
#include "bitstamp.h"
#include "debug.h"
#include "argo.h"
#include "store.h"
#include "common.h"

WATCHER *bitstamp_watcher_start(WATCHER_TYPE *type, char *args[]) {
    if (args[0] == NULL) {
        return NULL;
    }

    int fd_reading[2];
    int fd_writing[2];

    if (pipe(fd_reading) < 0) {
        return NULL;
    }
    if (pipe(fd_writing) < 0) {
        return NULL;
    }

    int pid = fork();
    if (pid == -1) {
        return NULL;
    }

    if (pid == 0) {
        close(fd_reading[0]);
        close(fd_writing[1]);
        if (dup2(fd_reading[1], 1) < 0) {
            exit(-1);
        }
        if (dup2(fd_writing[0], 0) < 0) {
            exit(-1);
        }
        execvp(type->argv[0], type->argv);
        return NULL;
    }

    close(fd_reading[1]);
    close(fd_writing[0]);
    COMMON_WATCHER *watcher = create_watcher(pid, fd_reading[0], fd_writing[1]);
    watcher->watcher_type = type;
    watcher->args[0] = strdup(args[0]);

    char buf[1024];
    strcpy(buf, "{\"event\":\"bts:subscribe\",\"data\":{\"channel\":\"");
    strcat(buf, args[0]);
    strcat(buf, "\"}}\n");
    bitstamp_watcher_send((WATCHER *)watcher, buf);
    return (WATCHER *)watcher;
}

int bitstamp_watcher_stop(WATCHER *wp) {
    COMMON_WATCHER *watcher = (COMMON_WATCHER *)wp;
    if (watcher->stop) {
        return 0;
    }

    watcher->stop = 1;
    if (kill(watcher->pid, SIGTERM) < 0) {
        info("kill %d", errno);
    }
    return 0;
}

int bitstamp_watcher_send(WATCHER *wp, void *arg) {
    COMMON_WATCHER *watcher = (COMMON_WATCHER *)wp;
    if (watcher->stop) {
        return -1;
    }

    return write(watcher->writing_fd, arg, strlen(arg));
}

int parse_trade_event(ARGO_VALUE *root, double *price, double *amount) {
    ARGO_VALUE *value = argo_value_get_member(root, "event");
    if (value == NULL) {
       return -1;
    }
    char *event = argo_value_get_chars(value);
    if (event == NULL) {
        return -1;
    }
    if (strcmp(event, "trade") != 0) {
        free(event);
        return -1;
    }
    free(event);

    value = argo_value_get_member(root, "data");
    if (value == NULL) {
       return -1;
    }
    ARGO_VALUE *elem = argo_value_get_member(value, "price");
    if (elem == NULL) {
        return -1;
    }
    if (argo_value_get_double(elem, price) < 0) {
        return -1;
    }
    elem = argo_value_get_member(value, "amount");
    if (elem == NULL) {
        return -1;
    }
    if (argo_value_get_double(elem, amount) < 0) {
        return -1;
    }

    return 0;
}

void update_bitstamp_data(COMMON_WATCHER *watcher, double price, double amount) {
    WATCHER_TYPE *type = (WATCHER_TYPE *)watcher->watcher_type;
    char price_key[256] = "";
    strcat(price_key, type->name);
    strcat(price_key, ":");
    strcat(price_key, watcher->args[0]);
    strcat(price_key, ":price");
    struct store_value value;
    value.type = STORE_DOUBLE_TYPE;
    value.content.double_value = price;
    store_put(price_key, &value);

    char volume_key[256] = "";
    strcat(volume_key, type->name);
    strcat(volume_key, ":");
    strcat(volume_key, watcher->args[0]);
    strcat(volume_key, ":volume");
    struct store_value *pv = store_get(volume_key);
    if (pv == NULL) {
        value.content.double_value = amount;
        store_put(volume_key, &value);
    } else {
        pv->content.double_value += amount;
        store_put(volume_key, pv);
        store_free_value(pv);
    }
}

void print_message(COMMON_WATCHER *watcher, const char *line) {
    if (!watcher->enable_trace) {
        return;
    }
    WATCHER_TYPE *type = (WATCHER_TYPE *)watcher->watcher_type;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    fprintf(stderr, "[%ld.%ld][%s][%2d][%5d]: %s", ts.tv_sec, ts.tv_nsec, type->name, watcher->reading_fd, watcher->serial_number, line);
}

int bitstamp_watcher_recv(WATCHER *wp, char *txt) {
    if (txt == NULL) {
        bitstamp_watcher_stop(wp);
        return 0;
    }

    COMMON_WATCHER *watcher = (COMMON_WATCHER *)wp;
    FILE *fp = create_memstream(watcher, txt);
    char line[1024];
    for (;;) {
        if (fgets(line, 1024, fp) == NULL) {
            close_memstream(watcher, NULL);
            return 0;
        }

        size_t size = strlen(line);
        if (line[size-1] != '\n') {
            close_memstream(watcher, line);
            break;
        }
        watcher->serial_number++;
        print_message(watcher, line);

        char *start = strstr(line, "Server message: '");
        if (start == NULL) {
            continue;
        }

        int len = 0;
        char *s = start+strlen("Server message: '");
        while (*s) {
            if (*s == '\'') {
                break;
            }
            len++;
            s++;
        }
        if (*s == '\0') {
            continue;
        }
        FILE *fp = fmemopen(start+strlen("Server message: '"), len, "r");
        ARGO_VALUE *root = argo_read_value(fp);
        if (root == NULL) {
            fclose(fp);
            continue;
        }
        double price, amount;
        if (parse_trade_event(root, &price, &amount) == 0) {
            update_bitstamp_data(watcher, price, amount);
        }
        argo_free_value(root);
        fclose(fp);
    }

    return 0;
}

int bitstamp_watcher_trace(WATCHER *wp, int enable) {
    COMMON_WATCHER *watcher = (COMMON_WATCHER *)wp;
    watcher->enable_trace = enable;
    return 0;
}
