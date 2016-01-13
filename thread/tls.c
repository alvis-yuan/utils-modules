#include <pthread.h>
#include <assert.h>


#define HAS_THREADS 1

#ifndef MAX_THREADS
#define MAX_THREADS 32
static int tls_flag[MAX_THREADS];
static void *tls[MAX_THREADS];
#endif


int mdl_thread_local_alloc(long *p_index)
{
#if HAS_THREADS
    pthread_key_t key;
    int rc;

    assert(p_index != NULL);

    if ((rc = pthread_key_create(&key, NULL)) != 0)
    {
        return rc;
    }

    *p_index = key;

    return 0;
#else
    int i;
    for (i = 0; i < MAX_THREADS; ++i)
    {
        if (tls_flag[i] == 0)
        {
            break;
        }
    }

    if (i == MAX_THREADS)
    {
        return -1;
    }

    tls_flag[i] = 1;
    tls[i] = NULL;

    *p_index = i;

    return 0;
#endif
}

/*
 * mdl_thread_local_free()
 */
void mdl_thread_local_free(long index)
{
#if HAS_THREADS
    pthread_key_delete(index);
#else
    tls_flag[index] = 0;
#endif
}

/*
 * mdl_thread_local_set()
 */
int mdl_thread_local_set(long index, void *value)
{
#if HAS_THREADS
    int rc = pthread_setspecific(index, value);

    return rc;
#else
    assert(index >= 0 && index < MAX_THREADS);
    tls[index] = value;

    return 0;
#endif
}

void *mdl_thread_local_get(long index)
{
#if HAS_THREADS
    return pthread_getspecific(index);
#else
    assert(index >= 0 && index < MAX_THREADS);

    return tls[index];
#endif
}

