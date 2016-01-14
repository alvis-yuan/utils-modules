#include "pool.h"


extern mdl_pool_factory_policy_t mdl_pool_factory_default_policy;
void mdl_pool_destroy_int(mdl_pool_t *pool);
mdl_pool_t *mdl_pool_create_int(mdl_pool_factory_t *factory, 
        const char *name,
        size_t initial_size, 
        size_t increment_size,
        mdl_pool_callback callback);

void mdl_pool_init_int(mdl_pool_t *pool, 
        const char *name,
        size_t increment_size,
        mdl_pool_callback callback);

void mdl_pool_reset(mdl_pool_t *pool);


static size_t pool_sizes[CACHING_POOL_ARRAY_SIZE] = 
{
    256, 512, 1024, 2048, 4096, 8192, 12288, 16384, 
    20480, 24576, 28672, 32768, 40960, 49152, 57344, 65536
};

#define START_SIZE  5

static mdl_pool_t* cpool_create_pool(mdl_pool_factory_t *pf, 
        const char *name, 
        size_t initial_size, 
        size_t increment_sz, 
        mdl_pool_callback callback)
{
    mdl_caching_pool_t *cp = (mdl_caching_pool_t*)pf;
    mdl_pool_t *pool;
    int idx;

    pthread_mutex_lock(&cp->lock);

    /* Use pool factory's policy when callback is NULL */
    if (callback == NULL) 
    {
        callback = pf->policy.callback;
    }

    if (initial_size <= pool_sizes[START_SIZE]) 
    {
        for (idx=START_SIZE-1; 
                idx >= 0 && pool_sizes[idx] >= initial_size;
                --idx)
        {
            ;
        }
        ++idx;
    } 
    else 
    {
        for (idx=START_SIZE+1; 
                idx < CACHING_POOL_ARRAY_SIZE && 
                pool_sizes[idx] < initial_size;
                ++idx)
        {
            ;
        }
    }

    /* Check whether there's a pool in the list. */
    if (idx == CACHING_POOL_ARRAY_SIZE || list_empty(&cp->free_list[idx])) 
    {
        /* No pool is available. */
        /* Set minimum size. */
        if (idx < CACHING_POOL_ARRAY_SIZE)
        {
            initial_size =  pool_sizes[idx];
        }

        /* Create new pool */
        pool = mdl_pool_create_int(&cp->factory, name, initial_size, 
                increment_sz, callback);
        if (!pool) 
        {
            pthread_mutex_unlock(&cp->lock);
            return NULL;
        }

    }
    else
    {
        /* Get one pool from the list. */
        pool = (mdl_pool_t *)cp->free_list[idx].next;
        list_del(&pool->pool_node_list);

        /* Initialize the pool. */
        mdl_pool_init_int(pool, name, increment_sz, callback);

        /* Update pool manager's free capacity. */
        if (cp->capacity > mdl_pool_get_capacity(pool)) 
        {
            cp->capacity -= mdl_pool_get_capacity(pool);
        }
        else 
        {
            cp->capacity = 0;
        }

        log(6, "%s: reuse pool size = %ld", pool->name, pool->capacity);
    }

    /* Put in used list. */
    list_add(&pool->pool_node_list, &cp->used_list);

    /* Mark factory data */
    pool->data_factory = (void *) (ssize_t)idx;

    /* Increment used count. */
    ++cp->used_count;

    pthread_mutex_unlock(&cp->lock);

    return pool;
}

static void cpool_release_pool(mdl_pool_factory_t *pf, mdl_pool_t *pool)
{
    mdl_caching_pool_t *cp = (mdl_caching_pool_t *)pf;
    size_t pool_capacity;
    unsigned i;

    pthread_mutex_lock(&cp->lock);

#if PJ_SAFE_POOL
    /* Make sure pool is still in our used list */
    if (list_find_node(&cp->used_list, pool) != pool) {
        pj_assert(!"Attempt to destroy pool that has been destroyed before");
        return;
    }
#endif

    /* Erase from the used list. */
    list_del(&pool->pool_node_list);

    /* Decrement used count. */
    --cp->used_count;

    pool_capacity = mdl_pool_get_capacity(pool);

    /* Destroy the pool if the size is greater than our size or if the total
     *      * capacity in our recycle list (plus the size of the pool) exceeds 
     *           * maximum capacity.
     *              . */
    if (pool_capacity > pool_sizes[CACHING_POOL_ARRAY_SIZE-1] ||
            cp->capacity + pool_capacity > cp->max_capacity)
    {
        mdl_pool_destroy_int(pool);
        pthread_mutex_unlock(&cp->lock);

        return;
    }

   log(6, "recycle(): cap=%ld, used=%ld(%ld%%)", 
                   pool_capacity, mdl_pool_get_used_size(pool), 
                           mdl_pool_get_used_size(pool)*100/pool_capacity);
    mdl_pool_reset(pool);

    pool_capacity = mdl_pool_get_capacity(pool);

    i = (unsigned) (unsigned long) (ssize_t) pool->data_factory;

    assert(i < CACHING_POOL_ARRAY_SIZE);
    if (i >= CACHING_POOL_ARRAY_SIZE ) 
    {
        /* Something has gone wrong with the pool. */
        mdl_pool_destroy_int(pool);
        pthread_mutex_unlock(&cp->lock);

        return;
    }

    list_add(&pool->pool_node_list, &cp->free_list[i]);
    cp->capacity += pool_capacity;

    pthread_mutex_unlock(&cp->lock);
}


static void cpool_dump_status(mdl_pool_factory_t *factory, int detail)
{
    mdl_caching_pool_t *cp = (mdl_caching_pool_t *)factory;

    pthread_mutex_lock(&cp->lock);

    log(3, "cachpool Dumping caching pool:");
    log(3, "cachpool  Capacity=%ld, max_capacity=%ld, used_cnt=%ld", 
                cp->capacity, cp->max_capacity, cp->used_count);
    if (detail) 
    {
        size_t total_used = 0, total_capacity = 0;
        struct list_head *node;
        mdl_pool_t      *pool;
        list_for_each(node, &cp->used_list)
        {
            pool = list_entry(node, mdl_pool_t, pool_node_list);
            size_t pool_capacity = mdl_pool_get_capacity(pool);
            log(3, "cachpool   %16s: %8ld of %8ld (%ld%%) used", 
                        mdl_pool_getobjname(pool), 
                        mdl_pool_get_used_size(pool), 
                        pool_capacity,
                        mdl_pool_get_used_size(pool)*100/pool_capacity);
            total_used += mdl_pool_get_used_size(pool);
            total_capacity += pool_capacity;
        }


        if (total_capacity) 
        {
            log(3, "cachpool  Total %9ld of %9ld (%ld %%) used!",
                        total_used, total_capacity,
                        total_used * 100 / total_capacity);
        }
    }

    pthread_mutex_unlock(&cp->lock);
}


static int cpool_on_block_alloc(mdl_pool_factory_t *f, size_t sz)
{
    mdl_caching_pool_t *cp = (mdl_caching_pool_t *)f;


    cp->used_size += sz;
    if (cp->used_size > cp->peak_used_size)
	cp->peak_used_size = cp->used_size;


    return 1;
}


static void cpool_on_block_free(mdl_pool_factory_t *f, size_t sz)
{
    mdl_caching_pool_t *cp = (mdl_caching_pool_t *)f;

    cp->used_size -= sz;
}



void mdl_caching_pool_init(mdl_caching_pool_t *cp, 
        const mdl_pool_factory_policy_t *policy,
        size_t max_capacity)
{
    int i;

    bzero(cp, sizeof(*cp));

    cp->max_capacity = max_capacity;
    INIT_LIST_HEAD(&cp->used_list);
    for (i=0; i < CACHING_POOL_ARRAY_SIZE; ++i)
    {
        INIT_LIST_HEAD(&cp->free_list[i]);
    }

    if (policy == NULL)
    {
        policy = &mdl_pool_factory_default_policy;
    }

    memcpy(&cp->factory.policy, policy, sizeof(mdl_pool_factory_policy_t));
    cp->factory.create_pool = &cpool_create_pool;
    cp->factory.release_pool = &cpool_release_pool;
    cp->factory.dump_status = &cpool_dump_status;
    cp->factory.on_block_alloc = &cpool_on_block_alloc;
    cp->factory.on_block_free = &cpool_on_block_free;

    pthread_mutex_init(&cp->lock, NULL);

    log(4, "create pool factory successfully!");
}

void mdl_caching_pool_destroy(mdl_caching_pool_t *cp)
{
    int i;
    mdl_pool_t *pool;
    struct list_head *node;

    /* Delete all pool in free list */
    for (i=0; i < CACHING_POOL_ARRAY_SIZE; ++i)
    {
        list_for_each(node, &cp->free_list[i])
        {
            pool = list_entry(node, mdl_pool_t, pool_node_list);
            list_del(node);
            mdl_pool_destroy_int(pool);
        }
    }

    /* Delete all pools in used list */
    list_for_each(node, &cp->used_list)
    {
        pool = list_entry(node, mdl_pool_t, pool_node_list);
        list_del(node);
        mdl_pool_destroy_int(pool);
    }

    pthread_mutex_destroy(&cp->lock);

    log(4, "detory pool factory!!!");
}



