#include "thread.h"


/* main thread to manage others */
static mdl_thread_t main_thread;
/* store thread id for application */
static long thread_tls_id;


/****************************************************************************/
/* thread local storage */
bool mdl_thread_local_alloc(long *p_index)
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


void mdl_thread_local_free(long index)
{
    pthread_key_delete(index);
}


bool mdl_thread_local_set(long index, void *value)
{
    int rc = pthread_setspecific(index, value);

    return rc == 0 ? SUCCESS : FAILURE;
}


void *mdl_thread_local_get(long index)
{
    return pthread_getspecific(index);
}

/* end of thread local storage */



/****************************************************************************/
/* thread interfaces */

bool mdl_thread_register (const char *cstr_thread_name,
        mdl_thread_t *desc,
        mdl_thread_t **ptr_thread)
{
    char stack_ptr;
    bool rc;
    mdl_thread_t *thread = desc;
    char *thread_name = (char*)cstr_thread_name;


    /* Warn if this thread has been registered before */
    if (mdl_thread_local_get(thread_tls_id) != 0) 
    {
        log(4, "Info: possibly re-registering existing thread");
    }

    //ASSERT_RETURN( (thread->thread == pthread_self()), FAILURE);

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
                "thread_%p", (void *)thread->thread);
    }

    rc = mdl_thread_local_set(thread_tls_id, thread);
    if (rc != SUCCESS) 
    {
        bzero(desc, sizeof(mdl_thread_t));
        return rc;
    }

    INIT_LIST_HEAD(&main_thread.node_thread);

#ifdef CHECK_STACK
    thread->stk_start = &stack_ptr;
    thread->stk_size = 0xFFFFFFFFUL;
    thread->stk_max_usage = 0;
#endif 
    
    *ptr_thread = thread;
    return SUCCESS;
}

bool mdl_thread_is_register(void)
{
    return mdl_thread_local_get(thread_tls_id) != NULL;
}


/* called in main thread to manage other threads */
bool mdl_thread_init(void)
{
    bool rc;
    mdl_thread_t *dummy;

    /* init entry in main thread, so called only once */
    rc = mdl_thread_local_alloc(&thread_tls_id);
    if (rc != SUCCESS) 
    {
        return rc;
    }

    return mdl_thread_register("thread_%p", &main_thread, &dummy);
}



static void *thread_main(void *param)
{
    mdl_thread_t *rec = (mdl_thread_t*)param;
    void *result;
    bool rc;

#ifdef CHECK_STACK
    rec->stk_start = (char*)&rec;
#endif

    /* Set current thread id. */
    rc = mdl_thread_local_set(thread_tls_id, rec);
    ASSERT_RETURN(rc == SUCCESS, NULL);

    rec->tid = syscall(SYS_gettid);

    list_add(&rec->node_thread, &main_thread.node_thread);

    /* Check if suspension is required. */
    if (rec->suspended_mutex) 
    {
        mdl_mutex_lock(rec->suspended_mutex);
        mdl_mutex_unlock(rec->suspended_mutex);
    }

    log(6, "Thread started", rec->obj_name);

    /* Call user's entry! */
    result = (void*)(long)(*rec->proc)(rec->arg);

    /* Done. */
    log(6, "Thread quitting", rec->obj_name);

    return result;
}



bool mdl_thread_create(mdl_pool_t *pool,
        const char *thread_name,
        thread_proc proc,
        void *arg,
        size_t stack_size,
        unsigned flags,
        mdl_thread_t **ptr_thread)
{
    mdl_thread_t *rec;

    /* every thread has its attribute so define not pointer */
    pthread_attr_t thread_attr;
    void *stack_addr;
    int rc;

    UNUSED_ARG(stack_addr);

    ASSERT_RETURN(pool && proc && ptr_thread, FAILURE);

    /* Create thread record and assign name for the thread */
    rec = (mdl_thread_t *)mdl_pool_zalloc(pool, sizeof(mdl_thread_t));
    ASSERT_RETURN(rec, FAILURE);

    /* Set name. */
    if (!thread_name)
    {
        thread_name = "thread_%p";
    }

    if (strchr(thread_name, '%')) 
    {
        snprintf(rec->obj_name, MAX_OBJ_NAME, thread_name, rec);
    } 
    else 
    {
        strncpy(rec->obj_name, thread_name, MAX_OBJ_NAME);
        rec->obj_name[MAX_OBJ_NAME - 1] = '\0';
    }

    /* Set default stack size */
    if (stack_size == 0)
    {
    //    stack_size = THREAD_DEFAULT_STACK_SIZE;
    }


#ifdef CHECK_STACK
    rec->stk_size = stack_size;
    rec->stk_max_usage = 0;
#endif

    /* Emulate suspended thread with mutex. */
    if (flags & THREAD_SUSPENDED) 
    {
        rc = mdl_mutex_create_simple(pool, NULL, &rec->suspended_mutex);
        if (rc != SUCCESS)
        {
            return rc;
        }

        mdl_mutex_lock(rec->suspended_mutex);
    } 
    else 
    {
        ASSERT_RETURN(rec->suspended_mutex == NULL, FAILURE);
    }


    /* Init thread attributes */
    pthread_attr_init(&thread_attr);

    /* Set thread's stack size */
    //rc = pthread_attr_setstacksize(&thread_attr, stack_size);
   // ASSERT_RETURN(rc == 0, FAILURE);



    /* Create the thread. */
    rec->proc = proc;
    rec->arg = arg;
    rc = pthread_create(&rec->thread, &thread_attr, &thread_main, rec);
    ASSERT_RETURN(rc == 0, FAILURE);

    *ptr_thread = rec;

    log(6, "%s Thread created", rec->obj_name);
    return SUCCESS;
}

void mdl_thread_dump(void)
{
    mdl_thread_t *tp;

    struct list_head *node;

    log(3, "(main thread) pid(tid): %ld, name: %s", 
            main_thread.tid, main_thread.obj_name);
    if (list_empty(&main_thread.node_thread))
    {
        return;
    }

    list_for_each(node, &main_thread.node_thread)
    {
        tp = list_entry(node, mdl_thread_t, node_thread);
        log(3, "tid: %ld, name: %s", tp->tid, tp->obj_name);
    }
}

char *mdl_thread_getname(mdl_thread_t *p)
{
    mdl_thread_t *rc = p;

    ASSERT_RETURN(p, "");

    return rc->obj_name;
}


bool mdl_thread_resume(mdl_thread_t *p)
{
    mdl_thread_t *rc = p;

    ASSERT_RETURN(p, FAILURE);

    return mdl_mutex_lock(rc->suspended_mutex);
}


mdl_thread_t *mdl_thread_this(void)
{
    return (mdl_thread_t *)mdl_thread_local_get(thread_tls_id);
}


bool mdl_thread_join(mdl_thread_t *p)
{
    mdl_thread_t *rec = (mdl_thread_t *)p;
    void *ret;
    int result;

    /* can't wait itself */
    ASSERT_RETURN(p != mdl_thread_this(), FAILURE);

    log(6, "%s Joining thread %s", mdl_thread_this()->obj_name, p->obj_name);
    result = pthread_join(rec->thread, &ret);
    CHECK_RETURN(result == 0, SUCCESS, FAILURE);
}


bool mdl_thread_destroy(mdl_thread_t *p)
{
    if (p->suspended_mutex)
    {
        mdl_mutex_destroy(p->suspended_mutex);
        p->suspended_mutex = NULL;
    }

    return SUCCESS;
}


bool mdl_thread_sleep(uint msec)
{
    struct timespec req;

    req.tv_sec = msec / 1000;
    req.tv_nsec = (msec % 1000) * 1000000;

    if (nanosleep(&req, NULL) == 0)
    {
        return SUCCESS;
    }

    return FAILURE;
}
/* end of thread interfaces */



/****************************************************************************/
/* mutex interfaces */
static bool init_mutex(mdl_mutex_t *mutex, const char *name, int type)
{
    pthread_mutexattr_t attr;
    int rc;

    rc = pthread_mutexattr_init(&attr);
    ENSURE_RETURN(rc == 0, FAILURE);

    if (type == MUTEX_SIMPLE)
    {
        rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
    }
    else
    {
        rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }
    ENSURE_RETURN(rc == 0, FAILURE);

    rc = pthread_mutex_init(&mutex->mutex, &attr);
    ENSURE_RETURN(rc == 0, FAILURE);

    rc = pthread_mutexattr_destroy(&attr);
    ENSURE_RETURN(rc == 0, FAILURE);

#ifdef MUTEX_DEBUG
    mutex->owner = NULL;
    mutex->owner_name[0] = '\0';
    mutex->nesting_level = 0;
#endif

    /* Set name. */
    if (!name)
    {
        name = "mtx%p";
    }
    if (strchr(name, '%')) 
    {
        snprintf(mutex->obj_name, MAX_OBJ_NAME, name, mutex);
    } 
    else 
    {
        strncpy(mutex->obj_name, name, MAX_OBJ_NAME);
        mutex->obj_name[MAX_OBJ_NAME-1] = '\0';
    }

    log(6, "%s Mutex created", mutex->obj_name);
    return SUCCESS;
}

bool mdl_mutex_create(mdl_pool_t *pool,
        const char *name,
        int type,
        mdl_mutex_t **ptr_mutex)
{
    bool rc;
    mdl_mutex_t *mutex;

    ASSERT_RETURN(pool && ptr_mutex, FAILURE);

    mutex = mdl_pool_alloc(pool, sizeof(mdl_mutex_t));
    ASSERT_RETURN(mutex, FAILURE);

    rc = init_mutex(mutex, name, type);
    ENSURE_RETURN(rc == SUCCESS, rc);

    *ptr_mutex = mutex;

    return SUCCESS;
}


bool mdl_mutex_create_simple(mdl_pool_t *pool,
        const char *name,
        mdl_mutex_t **mutex)
{
    return mdl_mutex_create(pool, name, MUTEX_SIMPLE, mutex);
}

bool mdl_mutex_create_recursive(mdl_pool_t *pool,
        const char *name,
        mdl_mutex_t **mutex)
{
    return mdl_mutex_create(pool, name, MUTEX_RECURSE, mutex);
}

bool mdl_mutex_lock(mdl_mutex_t *mutex)
{
    bool status;

    ASSERT_RETURN(mutex != NULL, FAILURE);  

#ifdef MUTEX_DEBUG
    log(6,"%s Mutex: thread %s is waiting (mutex owner=%s)",
           mutex->obj_name,
           mdl_thread_this()->obj_name,
           mutex->owner_name);

#endif
    
    status = pthread_mutex_lock(&mutex->mutex); 
#ifdef MUTEX_DEBUG
    if (status == 0)
    {
        ++mutex->nesting_level;
        mutex->owner = mdl_thread_this();
        snprintf(mutex->owner_name, MAX_OBJ_NAME, "%s", 
                mutex->owner->obj_name);
    }

    if (status == 0)
    {
        log(6, "%s Mutex acquired by thread %s (level=%d)", 
                mutex->obj_name, mdl_thread_this()->obj_name,
                mutex->nesting_level);
    }
    else
    {
        log(6, "%s Mutex acquisition FAILED by thread %s (level=%d)", 
                mutex->obj_name, mdl_thread_this()->obj_name,
                mutex->nesting_level);
    }
#else
    if (status == 0)
    {
        log(6, "%s Mutex acquired by thread %s", 
                mutex->obj_name, mdl_thread_this()->obj_name);
    }
    else
    {
        log(6, "%s Mutex acquisition FAILED by thread %s", 
                mutex->obj_name, mdl_thread_this()->obj_name);
    }
#endif

    CHECK_RETURN(status == 0, SUCCESS, FAILURE);
}


bool mdl_mutex_unlock(mdl_mutex_t *mutex)
{
    bool status;

    ASSERT_RETURN(mutex != NULL, FAILURE);

#ifdef MUTEX_DEBUG
    ASSERT_RETURN(mutex->owner == mdl_thread_this(), FAILURE);
    if (--mutex->nesting_level == 0)
    {
        mutex->owner_name[0] = '\0';
        mutex->owner = NULL;
    }
    log(6, "%s Mutex released by thread %s (level=%d)", 
            mutex->obj_name, mdl_thread_this()->obj_name,
            mutex->nesting_level);
#else
    log(6, "%s Mutex released by thread %s", 
            mutex->obj_name, mdl_thread_this()->obj_name);
#endif

    status = pthread_mutex_lock(&mutex->mutex);
    CHECK_RETURN(status == 0, SUCCESS, FAILURE);
}

bool mdl_mutex_trylock(mdl_mutex_t *mutex)
{
    bool status;

    ASSERT_RETURN(mutex != NULL, FAILURE);

    log(6, "%s thread %s trying lock", mutex->obj_name, 
            mdl_thread_this()->obj_name);

    status = pthread_mutex_trylock(&mutex->mutex);
    if (status == 0)
    {
#ifdef MUTEX_DEBUG
        mutex->owner = mdl_thread_this();
        strcpy(mutex->owner_name, mutex->owner->obj_name);
        ++mutex->nesting_level;

        log(6, "%s Mutex acquired by thread %s (level=%d)",
                    mutex->obj_name,
                    mdl_thread_this()->obj_name,
                    mutex->nesting_level);
#else
        log(6,"%s Mutex acquired by thread %s",
                mutex->obj_name, 
                mdl_thread_this()->obj_name);

#endif
    }
    else
    {
        log(6,"%s thread %s trylock failed",
                mutex->obj_name, 
                mdl_thread_this()->obj_name);
    }


    CHECK_RETURN(status == 0, SUCCESS, FAILURE);
}

bool mdl_mutex_destroy(mdl_mutex_t *mutex)
{
    enum { RETRY = 4 };
    int status = 0;
    unsigned retry;

    ASSERT_RETURN(mutex, FAILURE);

    log(6,"%s Mutex destroyed by thread %s",
            mutex->obj_name, 
            mdl_thread_this()->obj_name);

    for (retry = 0; retry < RETRY; ++retry)
    {
        status = pthread_mutex_destroy(&mutex->mutex);
        if (status == 0)
        {
            break;
        }
        else if (retry < RETRY - 1 && status == EBUSY)
        {
            pthread_mutex_unlock(&mutex->mutex);
        }
    }

    CHECK_RETURN(status == 0, SUCCESS, FAILURE);
}


bool mdl_mutex_islocked(mdl_mutex_t *mutex)
{
    ASSERT_RETURN(mutex != NULL, FALSE);

    return mutex->owner == mdl_thread_this();
}

/* end of mutex operations */


/****************************************************************************/
/* atomic operations */
bool mdl_atomic_create(mdl_pool_t *pool,
        atomic_value_t initial,
        mdl_atomic_t **ptr_atomic)
{
    bool rc;
    mdl_atomic_t *atomic_var;

    atomic_var = mdl_pool_alloc(pool, sizeof(mdl_atomic_t));

    ASSERT_RETURN(atomic_var, FAILURE);

    rc = mdl_mutex_create(pool, "atm%p", MUTEX_SIMPLE, &atomic_var->mutex);
    ENSURE_RETURN(rc == SUCCESS, rc);

    atomic_var->value = initial;

    *ptr_atomic = atomic_var;

    return SUCCESS;
}

bool mdl_atomic_destroy(mdl_atomic_t *atomic_var)
{
        ASSERT_RETURN(atomic_var, FAILURE);
        return mdl_mutex_destroy( atomic_var->mutex);
}

void mdl_atomic_set(mdl_atomic_t *atomic_var, atomic_value_t value)
{
    mdl_mutex_lock(atomic_var->mutex);
    atomic_var->value = value;
    mdl_mutex_unlock(atomic_var->mutex);
}

atomic_value_t mdl_atomic_get(mdl_atomic_t *atomic_var)
{
    atomic_value_t oldval;

    mdl_mutex_lock(atomic_var->mutex);
    oldval = atomic_var->value;
    mdl_mutex_unlock(atomic_var->mutex);

    return oldval;
}

atomic_value_t mdl_atomic_inc_and_get(mdl_atomic_t *atomic_var)
{
    atomic_value_t new_value;

    mdl_mutex_lock(atomic_var->mutex);
    new_value = ++atomic_var->value;
    mdl_mutex_unlock(atomic_var->mutex);

    return new_value;
}

void mdl_atomic_inc(mdl_atomic_t *atomic_var)
{
    mdl_atomic_inc_and_get(atomic_var);
}

atomic_value_t mdl_atomic_dec_and_get(mdl_atomic_t *atomic_var)
{
    atomic_value_t new_value;

    mdl_mutex_lock( atomic_var->mutex );
    new_value = --atomic_var->value;
    mdl_mutex_unlock( atomic_var->mutex);

    return new_value;
}

void mdl_atomic_dec(mdl_atomic_t *atomic_var)
{
    mdl_atomic_dec_and_get(atomic_var);
}

atomic_value_t mdl_atomic_add_and_get(mdl_atomic_t *atomic_var,
        atomic_value_t value )
{
    atomic_value_t new_value;

    mdl_mutex_lock(atomic_var->mutex);

    atomic_var->value += value;
    new_value = atomic_var->value;

    mdl_mutex_unlock(atomic_var->mutex);

    return new_value;
}

void mdl_atomic_add(mdl_atomic_t *atomic_var,
        atomic_value_t value)
{
    mdl_atomic_add_and_get(atomic_var, value);
}

/* endo of atomic operations */

/****************************************************************************/
/* semaphore operations */

bool mdl_sem_create(mdl_pool_t *pool,
        const char *name,
        unsigned initial,
        unsigned max,
        mdl_sem_t **ptr_sem)
{
    mdl_sem_t *sem;

    UNUSED_ARG(max);

    ASSERT_RETURN(pool != NULL && ptr_sem != NULL, FAILURE);

    sem = mdl_pool_alloc(pool, sizeof(mdl_sem_t));
    ASSERT_RETURN(sem, FAILURE);

    sem->sem = mdl_pool_alloc(pool, sizeof(sem_t));
    if (sem_init(sem->sem, 0, initial) != 0)
    {
        return FAILURE;
    }

    if (!name) 
    {
        name = "sem%p";
    }
    if (strchr(name, '%')) 
    {
        snprintf(sem->obj_name, MAX_OBJ_NAME, name, sem);
    }
    else 
    {
        strncpy(sem->obj_name, name, MAX_OBJ_NAME);
        sem->obj_name[MAX_OBJ_NAME-1] = '\0';
    }

    log(6, "%s Semaphore created", sem->obj_name);

    *ptr_sem = sem;
    return SUCCESS;
}

bool mdl_sem_destroy(mdl_sem_t *sem)
{
    int result;

    ASSERT_RETURN(sem, FAILURE);

    log(6, "%s Semaphore destroyed by thread %s",
            sem->obj_name, 
            mdl_thread_this()->obj_name);
    result = sem_destroy(sem->sem);

    CHECK_RETURN(result == 0, SUCCESS, FAILURE);
}

bool mdl_sem_wait(mdl_sem_t *sem)
{
    int result;

    ASSERT_RETURN(sem, FAILURE);

    log(6, "%s Semaphore: thread %s is waiting",
            sem->obj_name, 
            mdl_thread_this()->obj_name);

    result = sem_wait(sem->sem);

    if (result == 0) 
    {
        log(6, "%s Semaphore acquired by thread %s",
                sem->obj_name, 
                mdl_thread_this()->obj_name);
    } 
    else 
    {
        log(6, "%s Semaphore: thread %s FAILED to acquire",
                sem->obj_name, 
                mdl_thread_this()->obj_name);
    }

    CHECK_RETURN(result == 0, SUCCESS, FAILURE); 
}

bool mdl_sem_trywait(mdl_sem_t *sem)
{
    int result;

    ASSERT_RETURN(sem, FAILURE);

    result = sem_trywait( sem->sem );

    if (result == 0) 
    {
        log(6, "%s Semaphore acquired by thread %s",
                    sem->obj_name, 
                    mdl_thread_this()->obj_name);
    }

    CHECK_RETURN(result == 0, SUCCESS, FAILURE);
}

bool mdl_sem_post(mdl_sem_t *sem)
{
    int result;

    log(6, "%s Semaphore released by thread %s",
                sem->obj_name, 
                mdl_thread_this()->obj_name);

    result = sem_post(sem->sem);

    CHECK_RETURN(result == 0, SUCCESS, FAILURE);
}


/* end of semaphore operations */
