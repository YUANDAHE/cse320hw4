#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#include "ticker.h"
#include "cli.h"
#include "common.h"


void handle_sigchld(int sig_num, siginfo_t *siginfo, void *data) {
    int pid = waitpid(-1, NULL, 0);
    if (pid > 0) {
        remove_watcher(pid);
        if (get_watcher_num() == 1) {
            exit(0);
        }
    }
}

void handle_sigint(int sig_num, siginfo_t *siginfo, void *data) {
    printf("exit...\n");
    exit(0);
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
        return;
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

void test2() {
    set_sigchld_handle();
    set_sigint_handle();
    
    int pid = fork();
    if (pid == -1) {
        return;
    }

    if (pid == 0) {
        char **argv = (char *[]){"/root/test", "wss://ws.bitstamp.net", NULL};
        if (execvp(argv[0], argv+1) < 0) {
           printf("execvp fail\n");
        }
        return;
    }

    printf("child pid %d\n", pid);
    while (1) {
        sigset_t mask;
        sigemptyset(&mask);
        sigsuspend(&mask);
    }
}


void test333() {
    char *buf = NULL;
    size_t size = 0;
    FILE *fp = open_memstream(&buf, &size);

    for (int i = 0; i < 10; i++) {
        fwrite("hello world", 1, 12, fp);

        char b[100];
        int n = fread(b, 1, 100, fp);
        printf("read %d %s\n", n, b);
    }

    fclose(fp);
    free(buf);
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
    add_watcher(cli_watcher_start(&watcher_types[0], NULL));

    while (1) {
        sigset_t mask;
        sigemptyset(&mask);
        sigsuspend(&mask);
    }

    return 0;
}
