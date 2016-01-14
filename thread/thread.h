#ifndef _INCLUDE_THREAD_H_
#define _INCLUDE_THREAD_H_

#include "../module_common.h"
#include "../datastruct/list.h"
#include "../log/log.h"
#include "../pool/pool.h"

#define MUTEX_DEBUG 1
#define CHECK_STACK


#define THREAD_DEFAULT_STACK_SIZE 



typedef struct mdl_thread_s mdl_thread_t;
typedef struct mdl_mutex_s  mdl_mutex_t;
typedef struct mdl_sem_s    mdl_sem_t;
typedef struct mdl_atomic_s mdl_atomic_t;
typedef enum thread_create_flags_e thread_create_flags;
typedef enum mutex_type_e mutex_type;


typedef long atomic_value_t;
typedef void *(*thread_proc) (void *);

enum thread_create_flags_e
{
    THREAD_SUSPENDED = 1
} ;

enum mutex_type_e
{
    MUTEX_DEFAULT,
    MUTEX_SIMPLE,
    MUTEX_RECURSE
};



/* thread */
struct mdl_thread_s
{
    list_node       node_thread;

    char             obj_name[MAX_OBJ_NAME];

    pthread_t        thread;

    tid_t           tid;

    thread_proc     proc;

    void            *arg;

    mdl_mutex_t     *suspended_mutex;

#ifdef CHECK_STACK
    uint     stk_size;

    uint     stk_max_usage;

    char       *stk_start;
#endif
};


/* mutal exclusive lock */
struct mdl_mutex_s
{
    pthread_mutex_t     mutex;

    char                obj_name[MAX_OBJ_NAME];

#if MUTEX_DEBUG
    int             nesting_level;

    mdl_thread_t   *owner;

    char            owner_name[MAX_OBJ_NAME];
#endif
};

/* atomic operation */
struct mdl_atomic_s
{
    mdl_mutex_t        *mutex;

    atomic_value_t      value;
};



/* semaphore */
struct mdl_sem_s
{
    sem_t          *sem;

    char            obj_name[MAX_OBJ_NAME];
};

/* thread local storage */
bool mdl_thread_local_alloc(long *p_index);
void mdl_thread_local_free(long index);
bool mdl_thread_local_set(long index, void *value);
void *mdl_thread_local_get(long index);

/* thread interfaces */
bool mdl_thread_register (const char *cstr_thread_name,
        mdl_thread_t *desc,
        mdl_thread_t **ptr_thead);
bool mdl_thread_is_register(void);
bool mdl_thread_init(void);
bool mdl_thread_create(mdl_pool_t *pool,
        const char *thread_name,
        thread_proc proc,
        void *arg,
        size_t stack_size,
        unsigned flags,
        mdl_thread_t **ptr_thread);
char *mdl_thread_getname(mdl_thread_t *p);
bool mdl_thread_resume(mdl_thread_t *p);
mdl_thread_t *mdl_thread_this(void);
bool mdl_thread_join(mdl_thread_t *p);
bool mdl_thread_destroy(mdl_thread_t *p);
bool mdl_thread_sleep(uint msec);
void mdl_thread_dump(void);

/* mutex interfaces */
bool mdl_mutex_create(mdl_pool_t *pool,
        const char *name,
        int type,
        mdl_mutex_t **ptr_mutex);
bool mdl_mutex_create_simple(mdl_pool_t *pool,
        const char *name,
        mdl_mutex_t **mutex);
bool mdl_mutex_create_recursive(mdl_pool_t *pool,
        const char *name,
        mdl_mutex_t **mutex);
bool mdl_mutex_lock(mdl_mutex_t *mutex);
bool mdl_mutex_unlock(mdl_mutex_t *mutex);
bool mdl_mutex_trylock(mdl_mutex_t *mutex);
bool mdl_mutex_destroy(mdl_mutex_t *mutex);
bool mdl_mutex_islocked(mdl_mutex_t *mutex);

/* atomic interfaces */
bool mdl_atomic_create(mdl_pool_t *pool,
        atomic_value_t initial,
        mdl_atomic_t **ptr_atomic);
bool mdl_atomic_destroy(mdl_atomic_t *atomic_var);
void mdl_atomic_set(mdl_atomic_t *atomic_var, atomic_value_t value);
atomic_value_t mdl_atomic_get(mdl_atomic_t *atomic_var);
atomic_value_t mdl_atomic_inc_and_get(mdl_atomic_t *atomic_var);
void mdl_atomic_inc(mdl_atomic_t *atomic_var);
atomic_value_t mdl_atomic_dec_and_get(mdl_atomic_t *atomic_var);
void mdl_atomic_dec(mdl_atomic_t *atomic_var);
atomic_value_t mdl_atomic_add_and_get(mdl_atomic_t *atomic_var,
        atomic_value_t value );
void mdl_atomic_add(mdl_atomic_t *atomic_var,
        atomic_value_t value);


/* semaphore interfaces */
bool mdl_sem_create(mdl_pool_t *pool,
        const char *name,
        unsigned initial,
        unsigned max,
        mdl_sem_t **ptr_sem);
bool mdl_sem_destroy(mdl_sem_t *sem);
bool mdl_sem_wait(mdl_sem_t *sem);
bool mdl_sem_trywait(mdl_sem_t *sem);
bool mdl_sem_post(mdl_sem_t *sem);







#endif /* _INCLUDE_THREAD_H_ */
