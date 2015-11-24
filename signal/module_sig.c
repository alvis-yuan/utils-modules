#include "../module_common.h"
#include "../log/module_log.h"

static void handler_callback(int sig)
{
    //printf("Thread [%ld] receive signal [%d](%s)\n", syscall(__NR_gettid), sig, strsignal(sig));
    module_log_error(MODULE_LOG_DEBUG, test_log, 0, "thread [%ld] receive signal [%d](%s)\n", syscall(__NR_gettid), sig, strsignal(sig));
}

static void *handler_sys_syn(void *arg)
{
    //printf("signal thread %ld\n", syscall(__NR_gettid));
    module_log_error(MODULE_LOG_DEBUG, test_log, 0, "signal thread %ld start...\n", syscall(__NR_gettid));
    int ret;
    int sig;
    sigset_t *set = arg;

    while (1)
    {
        ret = sigwait(set, &sig);
        if (!ret)
        {
            handler_callback(sig);
        }
        else
        {
            printf("sigwait: %s\n", strerror(errno));
        }
    }

    return NULL;
}

void module_sig_mask(int *sigs, int cnt)
{
    int i = 0;
    sigset_t set;
    sigemptyset(&set);

    for (i = 0; i < cnt; ++i)
    {
        sigaddset(&set, sigs[i]);
    }

    pthread_sigmask(SIG_BLOCK, &set, NULL);

    /* 
     * install default signal handler, but if want to synchronize with sigwait 
     * because sigwait prior to intall handler
     */
    for (i = 0; i < cnt; ++i)
    {
        signal(sigs[i], handler_callback);
    }
}

int module_sig_syn(pthread_t *tid, int *sigs, int cnt)
{
    int i;
    int ret;
    sigset_t set;

    sigemptyset(&set);

    for (i = 0; i < cnt; ++i)
    {
        sigaddset(&set, sigs[i]);
    }

    ret = pthread_create(tid, NULL, handler_sys_syn, &set);
    if (ret)
    {
        return ret;
    }

    return 0;
}

/* you can reinstall signal handler
 * e.g. SIGSEGV, synchonize it meaningless, 
 * then you can install handler with signal/sigaction to catch
 */
void module_sig_asyn(int sig, handler_sig func)
{
    signal(sig, func);
}

