#include "pool.h"
#include "sigature.h"

static void *default_block_alloc(mdl_pool_factory_t *factory, size_t size)
{
    void *p;

    if (factory->on_block_alloc)
    {
        int rc;
        rc = factory->on_block_alloc(factory, size);
        if (!rc)
        {
            return NULL;
        }
    }

    /* allocate size + 2  * SIG_SIZE */
    p = malloc(size+(SIG_SIZE << 1));

    if (p == NULL) 
    {
        if (factory->on_block_free) 
        {
            factory->on_block_free(factory, size);
        }
    } 
    else
    {
        APPLY_SIG(p, size);
    }

    log(3, "malloc memory addr = %p, size = %ld", p, size);
    return p;
}

static void default_block_free(mdl_pool_factory_t *factory, void *mem, 
        size_t size)
{
    if (factory->on_block_free) 
    {
        factory->on_block_free(factory, size);
    }

    REMOVE_SIG(mem, size);

    free(mem);

    log(3, "free memory addr = %p, size = %ld", mem, size);
}

static int default_pool_callback(mdl_pool_t *pool, size_t size)
{
    UNUSED_ARG(pool);
    UNUSED_ARG(size);

    return ENOMEM;
}


mdl_pool_factory_policy_t mdl_pool_factory_default_policy =
{
    &default_block_alloc,
    &default_block_free,
    &default_pool_callback,
    0
};

const mdl_pool_factory_policy_t *mdl_pool_factory_get_default_policy(void)
{
        return &mdl_pool_factory_default_policy;
}

