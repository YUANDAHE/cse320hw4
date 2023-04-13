#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#include "ticker.h"
#include "cli.h"
#include "common.h"
#include "argo.h"
#include "debug.h"

void handle_sigchld(int sig_num, siginfo_t *siginfo, void *data) {
    int pid = waitpid(-1, NULL, 0);
    if (pid > 0) {
        remove_watcher(pid);
        if (is_quit()) {
            if (get_watcher_num() == 1) {
                exit(0);
            } else {
                stop_one_watcher();
            }
        }
    }
}

void handle_sigint(int sig_num, siginfo_t *siginfo, void *data) {
   quit();
}

void handle_sigio(int sig_num, siginfo_t *siginfo, void *data) {
    COMMON_WATCHER *watcher = find_watcher_by_reading_fd(siginfo->si_fd);
    if (watcher == NULL) {
        return;
    }

    char buf[1024];
    while (1) {
        int n = read(siginfo->si_fd, buf, 1023);
        if (n > 0) {
            buf[n] = 0;
            ((WATCHER_TYPE *)watcher->watcher_type)->recv((WATCHER *)watcher, buf);
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        ((WATCHER_TYPE *)watcher->watcher_type)->recv((WATCHER *)watcher, NULL);
        break;
    }
}

int set_sigchld_handle() {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = handle_sigchld;

    return sigaction(SIGCHLD, &act, NULL);
}

int set_sigint_handle() {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = handle_sigint;

    return sigaction(SIGINT, &act, NULL);
}

int set_sigio_handle() {
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = handle_sigio;

    return sigaction(SIGIO, &act, NULL);
}

int ticker(void) {
    if (set_sigchld_handle()) {
        return -1;
    }
    if (set_sigio_handle()) {
        return -1;
    }
    if (set_sigint_handle()) {
        return -1;
    }
    
    init_watcher();
    WATCHER *watcher = cli_watcher_start(&watcher_types[0], NULL);
    add_watcher(watcher);

    char buf[1024];
    int n = read(STDIN_FILENO, buf, 1023);
    if (n > 0) {
        buf[n] = 0;
        WATCHER_TYPE *type = (WATCHER_TYPE *)((COMMON_WATCHER *)watcher)->watcher_type;
        type->recv(watcher, buf);
    } else if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 0;
    }
    
    while (1) {
        sigset_t set;
        sigemptyset(&set);
        sigsuspend(&set);
    }

    return 0;
}
