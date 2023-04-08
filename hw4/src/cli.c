#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ticker.h"
#include "store.h"
#include "cli.h"
#include "debug.h"
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
    return write(1, arg, strlen(arg));
}

int cli_watcher_recv(WATCHER *wp, char *txt) {
    COMMON_WATCHER *watcher = (COMMON_WATCHER *)wp;
    size_t size = strlen(txt);
    fwrite(txt, 1, size, watcher->input);
    int newline = 0;
    for (size_t i = 0; i < size; i++) {
        if (txt[i] == '\n') {
            newline = 1;
            break;
        }
    }

    if (!newline) {
        return 0;
    }

    char line[1024];
    if (fgets(line, 1024, watcher->input) == NULL) {
        return 0;
    }

    if (strcmp(line, "watchers\n") == 0) {
        print_watchers();
    } else if (strcmp(line, "quit\n") == 0) {
        stop_all_watchers();
        if (get_watcher_num() == 1) {
            exit(0);
        }
    } else {
        printf("???\n");
    }

    cli_watcher_send((WATCHER *)watcher, "ticker> ");
    return 0;
}

int cli_watcher_trace(WATCHER *wp, int enable) {
    return 0;
}
