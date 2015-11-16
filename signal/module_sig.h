#ifndef _SIG_MODULE_H_
#define _SIG_MODULE_H_

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/syscall.h>

typedef void (*handler_sig)(int sig);

typedef struct {
    int signo;
    char *signame;
    handler_sig handler;
}module_sig_t;

void module_sig_mask(int *sigs, int cnt);
int module_sig_syn(pthread_t *tid, int *sigs, int cnt);
void module_sig_asyn(int sig, handler_sig func);

#endif /* _SIG_MODULE_H_ */













