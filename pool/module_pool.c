#include "module_pool.h"


void *module_alloc(size_t size, module_log_t *log)
{
    void  *p;

    if (!(p = malloc(size))) {
    //    module_log_error(MODULE_LOG_EMERG, log, module_errno,
     //           "malloc() " SIZE_T_FMT " bytes failed", size);
    }

    //module_log_debug2(MODULE_LOG_DEBUG_ALLOC, log, 0,
     //       "malloc: " PTR_FMT ":" SIZE_T_FMT, p, size);

    return p;
}



void *module_calloc(size_t size, module_log_t *log)
{
    void  *p;

    p = module_alloc(size, log);

    if (p) {
        memset(p, 0, size);
    }

    return p;
}


module_pool_t *module_create_pool(size_t size, module_log_t *log)
{
    module_pool_t  *p;

    if (!(p = module_alloc(size, log))) {
        return NULL;
    }

    p->last = (char *) p + sizeof(module_pool_t);
    p->end = (char *) p + size;
    p->next = NULL;
    p->large = NULL;
    p->log = log;

    return p;
}



void module_destroy_pool(module_pool_t *pool)
{
    module_pool_t        *p, *n;
    module_pool_large_t  *l;

    for (l = pool->large; l; l = l->next) {
        if (l->alloc) {
            free(l->alloc);
        }
    }

    for (p = pool, n = pool->next; /* void */; p = n, n = n->next) {
        free(p);

        if (n == NULL) {
            break;
        }
    }
}

void *module_palloc(module_pool_t *pool, size_t size)
{
    char              *m;
    module_pool_t        *p, *n;
    module_pool_large_t  *large, *last;

    if (size <= (size_t) MODULE_MAX_ALLOC_FROM_POOL
            && size <= (size_t) (pool->end - (char *) pool) - sizeof(module_pool_t))
    {
        for (p = pool, n = pool->next; /* void */; p = n, n = n->next) {
            m = module_align(p->last);

            if ((size_t) (p->end - m) >= size) {
                p->last = m + size ;

                return m;
            }

            if (n == NULL) {
                break;
            }
        }

        /* allocate a new pool block */

        if (!(n = module_create_pool((size_t) (p->end - (char *) p), p->log))) {
            return NULL;
        }

        p->next = n;
        m = n->last;
        n->last += size;

        return m;
    }

    /* allocate a large block */

    large = NULL;
    last = NULL;

    if (pool->large) {
        for (last = pool->large; /* void */ ; last = last->next) {
            if (last->alloc == NULL) {
                large = last;
                last = NULL;
                break;
            }

            if (last->next == NULL) {
                break;
            }
        }
    }

    if (large == NULL) {
        if (!(large = module_palloc(pool, sizeof(module_pool_large_t)))) {
            return NULL;
        }

        large->next = NULL;
    }

    if (!(p = module_alloc(size, pool->log))) {
        return NULL;
    }

    if (pool->large == NULL) {
        pool->large = large;

    } else if (last) {
        last->next = large;
    }

    large->alloc = p;

    return p;
}



void *module_pcalloc(module_pool_t *pool, size_t size)
{
    void *p;

    p = module_palloc(pool, size);
    if (p) {
        memset(p, 0, size);
    }

    return p;
}



int module_pfree(module_pool_t *pool, void *p)
{
    module_pool_large_t  *l;

    for (l = pool->large; l; l = l->next) {
        if (p == l->alloc) {
            //module_log_debug1(MODULE_LOG_DEBUG_ALLOC, pool->log, 0,
             //       "free: " PTR_FMT, l->alloc);
            free(l->alloc);
            l->alloc = NULL;

            return 0;
        }
    }

    return -1;
}
