#ifndef _INCLUDE_THREAD_H_
#define _INCLUDE_THREAD_H_

#include "../module_common.h"
#include "../datastruct/list.h"

#define DEBUG 1



typedef struct mdl_thread_s mdl_thread_t;
typedef struct mdl_mutex_s  mdl_mutex_t;
typedef struct mdl_sem_s    mdl_sem_t;
typedef struct mdl_atomic_s mdl_atomic_t;


typedef long atomic_value_t;
typedef void *(*thread_proc) (void *);

/* thread */
struct mdl_thread_s
{
    list_node       node_thread;

    char             obj_name[MAX_OBJ_NAME];

    pthread_t        thread;

    tid_t           tid;

    thread_proc     *proc;

    void            *arg;

    mdl_mutex_t     *suspended_mutex;
};


/* mutal exclusive lock */
struct mdl_mutex_s
{
    pthread_mutex_t     mutex;

    char                obj_name[MAX_OBJ_NAME];

#if DEBUG
    int             nesting_level;

    mdl_thread_t   *owner;

    char            owner_name[MAX_OBJ_NAME];
#endif
};

/* atomic number */
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













#endif /* _INCLUDE_THREAD_H_ */
