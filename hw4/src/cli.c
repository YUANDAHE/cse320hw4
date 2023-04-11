#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "ticker.h"
#include "store.h"
#include "cli.h"
#include "debug.h"
#include "store.h"
#include "common.h"

WATCHER *cli_watcher_start(WATCHER_TYPE *type, char *args[]) {
    COMMON_WATCHER *watcher = create_watcher(-1, 0, 1);
    watcher->watcher_type = type;
    cli_watcher_send((WATCHER *)watcher, "ticker> ");

    return (WATCHER *)watcher;
}

int cli_watcher_stop(WATCHER *wp) {
    return 0;
}

int cli_watcher_send(WATCHER *wp, void *arg) {
    printf("%s", (char*)arg);
    fflush(stdout);
    return 0;
}

void parse_cmd(char *s, char **args) {
    char *str = s;
    int index = 0;
    int start = 0;
    while (*str) {
        char c = *str;
        str++;
        if (c == '\n') {
            return;
        }
        if (c == ' ' || c == '\t') {
            start++;
            continue;
        }

        int end = start + 1;
        while (*str) {
            char c = *str;
            str++;
            if (c == ' ' || c == '\t' || c == '\n') {
                break;
            }
            end++;
        }
        s[end] = 0;
        args[index++] = s + start;
        start = end + 1;
    }
}

void do_start(char **args) {
    if (args[1] == NULL) {
        printf("???\n");
        return;
    }
    int i = 0;
    while (1) {
        if (watcher_types[i].name == NULL) {
            printf("???\n");
            return;
        }
        if (strcmp(args[1], watcher_types[i].name) == 0) {
            break;
        }
        i++;
    }

    if (args[2] == NULL) {
        printf("???\n");
        return;
    } 

    WATCHER *watcher = watcher_types[i].start(&watcher_types[i], args+2);
    if (watcher == NULL) {
        printf("???\n");
        return;
    }
    add_watcher(watcher);
}

void do_stop(char **args) {
    if (args[1] == NULL) {
        printf("???\n");
        return;
    }

    char *s = args[1];
    while (*s) {
        if (!isdigit(*s)) {
            printf("???\n");
            return;
        }
        s++;
    }

    int index = atoi(args[1]);
    if (index == 0) {
        printf("???\n");
        return;
    }
   COMMON_WATCHER *watcher = get_watcher(index);
    if (watcher == NULL) {
        printf("???\n");
        return;
    }
    WATCHER_TYPE *type = (WATCHER_TYPE *)watcher->watcher_type;
    type->stop((WATCHER *)watcher);
}

void do_show(char **args) {
    if (args[1] == NULL) {
        printf("???\n");
        return;
    }

    struct store_value *pv = store_get(args[1]);
    if (pv == NULL) {
        printf("???\n");
        return;
    }

    if (pv->type == STORE_INT_TYPE) {
        printf("%s\t%d\n", args[1], pv->content.int_value);
    } else if (pv->type == STORE_LONG_TYPE) {
        printf("%s\t%ld\n", args[1], pv->content.long_value);
    } else if (pv->type == STORE_DOUBLE_TYPE) {
        printf("%s\t%f\n", args[1], pv->content.double_value);
    } else if (pv->type == STORE_STRING_TYPE) {
        printf("%s\t%s\n", args[1], pv->content.string_value);
    } else {
        printf("???\n");
    }

    store_free_value(pv);
}

void do_trace(char **args, int enable) {
    if (args[1] == NULL) {
        printf("???\n");
        return;
    }

    char *s = args[1];
    while (*s) {
        if (!isdigit(*s)) {
            printf("???\n");
            return;
        }
        s++;
    }

    int index = atoi(args[1]);
    if (index == 0) {
        printf("???\n");
        return;
    }

    COMMON_WATCHER *watcher = get_watcher(index);
    if (watcher == NULL) {
        printf("???\n");
        return;
    }
    WATCHER_TYPE *type = (WATCHER_TYPE *)watcher->watcher_type;
    type->trace((WATCHER *)watcher, enable);
}

int cli_watcher_recv(WATCHER *wp, char *txt) {
    if (is_quit()) {
        return 0;
    }

    if (txt == NULL) {
        quit();
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

        size_t len = strlen(line);
        if (line[len-1] != '\n') {
            close_memstream(watcher, line);
            return 0;
        }

        char *args[100] = {NULL};
        parse_cmd(line, args);
        if (args[0] == NULL) {
            printf("???\n");
            cli_watcher_send((WATCHER *)watcher, "ticker> ");
            continue;
        }

        if (strcmp(args[0], "watchers") == 0) {
            print_watchers();
        } else if (strcmp(args[0], "quit") == 0) {
            if (get_watcher_num() == 1) {
                cli_watcher_send((WATCHER *)watcher, "ticker> ");
            }
            quit();
        } else if (strcmp(args[0], "start") == 0) {
            do_start(args);
        } else if (strcmp(args[0], "stop") == 0) {
            do_stop(args);
        } else if (strcmp(args[0], "show") == 0 ) {
            do_show(args);
        } else if (strcmp(args[0], "trace") == 0 ) {
            do_trace(args, 1);
        } else if (strcmp(args[0], "untrace") == 0 ) {
            do_trace(args, 0);
        } else {
            printf("???\n");
        }

        cli_watcher_send((WATCHER *)watcher, "ticker> ");
    }
    
    close_memstream(watcher, NULL);
    return 0;
}

int cli_watcher_trace(WATCHER *wp, int enable) {
    return 0;
}
