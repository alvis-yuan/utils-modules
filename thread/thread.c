#include "thread.h"
#include "../log/log.h"
#include "../pool/pool.h"


/* main thread to manage others */
static mdl_thread_t main_thread;
/* store thread id for application */
static long thread_tls_tid;


/* thread local storage */

bool thread_local_alloc(long *p_index)
{
    pthread_key_t key;
    int rc;

    ASSERT_RETURN(p_index != NULL, FAILURE);

    if ( ( rc = pthread_key_create(&key, NULL) ) != 0 )
    {
        return FAILURE;
    }

    *p_index = key;

    return SUCCESS;
}


void thread_local_free(long index)
{
    pthread_key_delete(index);
}


bool thread_local_set(long index, void *value)
{
    int rc = pthread_setspecific(index, value);

    return rc == 0 ? SUCCESS : FAILURE;
}


void *thread_local_get(long index)
{
    return pthread_getspecific(index);
}



/* thread interfaces */

/* called in main thread to manage other threads */
bool mdl_thread_register (const char *cstr_thread_name,
        mdl_thread_t *desc,
        bool mthread)
{
    bool rc;
    mdl_thread_t *thread = desc;
    char *thread_name = (char*)cstr_thread_name;


    /* Warn if this thread has been registered before */
    if (thread_local_get(thread_tls_tid) != 0) 
    {
        log(4, "Info: possibly re-registering existing thread");
    }

    ASSERT_RETURN( (thread->thread == pthread_self()), FAILURE);

    /* Initialize and set the thread entry. */
    bzero(desc, sizeof(mdl_thread_t));
    thread->thread = pthread_self();
    thread->tid = syscall(SYS_gettid);

    if (cstr_thread_name && strlen(thread_name) < sizeof(thread->obj_name)-1)
    {
        snprintf(thread->obj_name, sizeof(thread->obj_name),
                cstr_thread_name, thread->thread);
    }
    else
    {
        snprintf(thread->obj_name, sizeof(thread->obj_name),
                "thread_%ld", thread->tid);
    }

    rc = thread_local_set(thread_tls_tid, thread);
    if (rc != SUCCESS) 
    {
        bzero(desc, sizeof(mdl_thread_t));
        return rc;
    }

    if (!mthread)
    {
        list_add(&thread->node_thread, &main_thread.node_thread);
    }
    else
    {
        INIT_LIST_HEAD(&main_thread.node_thread);
    }

    return SUCCESS;
}


/* thread constructor */
bool mdl_thread_init(void)
{
    bool rc;

    rc = thread_local_alloc(&thread_tls_tid);
    if (rc != SUCCESS) 
    {
        return rc;
    }

    return mdl_thread_register("thr%p", &main_thread, TRUE);
}





