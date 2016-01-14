#include "pool.h"
#include "../datastruct/list.h"

#define ALIGN_PTR(PTR,ALIGNMENT)    (PTR + (-(ssize_t)(PTR) & (ALIGNMENT - 1)))
#define MAX_OBJ_NAME    32


static mdl_pool_block_t *mdl_pool_create_block(mdl_pool_t *pool, size_t size)
{
    mdl_pool_block_t *block;

    assert(size >= sizeof(mdl_pool_block_t));


    /* Request memory from allocator. */
    block = (mdl_pool_block_t *) 
        (*pool->factory->policy.block_alloc)(pool->factory, size);
    if (block == NULL) 
    {
        (*pool->callback)(pool, size);

        return NULL;
    }

    /* Add capacity. */
    pool->capacity += size;

    /* Set start and end of buffer. */
    block->start = ((unsigned char*)block) + sizeof(mdl_pool_block_t);
    block->end = ((unsigned char*)block) + size;

    /* Set the start pointer, aligning it as needed */
    block->current = ALIGN_PTR(block->start, POOL_ALIGNMENT);

    /* Insert in the front of the list. */
    list_add(&block->block_node_list, &pool->block_head_list);

    return block;
}

void *mdl_pool_allocate_find(mdl_pool_t *pool, size_t size)
{
    struct list_head *node;
    mdl_pool_block_t *block;
    void *p;
    size_t block_size;


    list_for_each(node, &pool->block_head_list)
    {
        block = list_entry(node, mdl_pool_block_t, block_node_list);
        p = mdl_pool_alloc_from_block(block, size);
        if (p != NULL)
        {
            return p;
        }
    }


    /* No available space in all blocks. */

    /* If pool is configured NOT to expand, return error. */
    if (pool->increment_size == 0) 
    {
        (*pool->callback)(pool, size);
        return NULL;
    }

    if (pool->increment_size < 
            size + sizeof(mdl_pool_block_t) + POOL_ALIGNMENT) 
    {
        size_t count;
        count = (size + pool->increment_size + sizeof(mdl_pool_block_t) +
                POOL_ALIGNMENT) / 
            pool->increment_size;
        block_size = count * pool->increment_size;

    }
    else 
    {
        block_size = pool->increment_size;
    }

    block = mdl_pool_create_block(pool, block_size);
    if (!block)
    {
        return NULL;
    }

    p = mdl_pool_alloc_from_block(block, size);
    assert(p != NULL);

    return p;
}



void mdl_pool_init_int(mdl_pool_t *pool, 
        const char *name,
        size_t increment_size,
        mdl_pool_callback callback)
{
    pool->increment_size = increment_size;
    pool->callback = callback;

    if (name) 
    {
        strncpy(pool->name, name, MAX_OBJ_NAME);
        pool->name[MAX_OBJ_NAME-1] = '\0';
    }
    else
    {
        pool->name[0] = '\0';
    }
}


mdl_pool_t *mdl_pool_create_int(mdl_pool_factory_t *f, const char *name,
        size_t initial_size, 
        size_t increment_size,
        mdl_pool_callback callback)
{
    mdl_pool_t *pool;
    mdl_pool_block_t *block;
    unsigned char *buffer;


    /* Size must be at least sizeof(pj_pool)+sizeof(pj_pool_block) */
    assert(initial_size >= sizeof(mdl_pool_t) + sizeof(mdl_pool_block_t));

    /* If callback is NULL, set calback from the policy */
    if (callback == NULL)
    {
        callback = f->policy.callback;
    }

    /* Allocate initial block */
    buffer = (unsigned char *) (*f->policy.block_alloc)(f, initial_size);
    if (!buffer)
    {
        return NULL;
    }

    /* Set pool administrative data. */
    pool = (mdl_pool_t *)buffer;
    bzero(pool, sizeof(*pool));

    INIT_LIST_HEAD(&pool->block_head_list);
    pool->factory = f;

    /* Create the first block from the memory. */
    block = (mdl_pool_block_t *) (buffer + sizeof(*pool));
    block->start = ((unsigned char*)block) + sizeof(mdl_pool_block_t);
    block->end = buffer + initial_size;

    /* Set the start pointer, aligning it as needed */
    block->current = ALIGN_PTR(block->start, POOL_ALIGNMENT);

    list_add(&block->block_node_list, &pool->block_head_list);

    mdl_pool_init_int(pool, name, increment_size, callback);

    /* Pool initial capacity and used size */
    pool->capacity = initial_size;

    log(4, "created pool[%s]: (size = %ld)", pool->name, pool->capacity);

    return pool;
}


static void reset_pool(mdl_pool_t *pool)
{
    mdl_pool_block_t *block;
    struct list_head *node;

    if (list_empty(&pool->block_head_list))
    {
        return ;
    }

    list_for_each_prev(node, &pool->block_head_list)
    {
        block = list_entry(node, mdl_pool_block_t, block_node_list);
        if (node->prev == &pool->block_head_list)
        {
            break;
        }

        list_del(&block->block_node_list);
        (*pool->factory->policy.block_free)(pool->factory, block, 
                block->end - (unsigned char*)block);
    }


    /* Set the start pointer, aligning it as needed */
    block->current = ALIGN_PTR(block->start, POOL_ALIGNMENT);

    pool->capacity = block->end - (unsigned char*)pool;
}


void mdl_pool_reset(mdl_pool_t *pool)
{
    reset_pool(pool);
}


void mdl_pool_destroy_int(mdl_pool_t *pool)
{
    size_t initial_size;
    mdl_pool_block_t *block = (mdl_pool_block_t *)pool->block_head_list.next;

    reset_pool(pool);

    initial_size = block->end - (unsigned char *)pool;

    if (pool->factory->policy.block_free)
    {
        (*pool->factory->policy.block_free)(pool->factory, pool, initial_size);
    }
}



size_t mdl_pool_get_capacity(mdl_pool_t *pool)
{
    return pool->capacity;
}

size_t mdl_pool_get_used_size(mdl_pool_t *pool)
{
    size_t used_size = sizeof(mdl_pool_t);
    struct list_head *node;
    mdl_pool_block_t *block;

    list_for_each(node, &pool->block_head_list)
    {
        block = list_entry(node, mdl_pool_block_t, block_node_list);
        used_size += (block->current - block->start) + sizeof(mdl_pool_block_t);
    }

    return used_size;
}

void *mdl_pool_alloc_from_block(mdl_pool_block_t *block, size_t size)
{
    if (size & (POOL_ALIGNMENT - 1))
    {
        size = (size + POOL_ALIGNMENT) & ~(POOL_ALIGNMENT - 1);
    }
    if ((size_t)(block->end - block->current) >= size) 
    {
        void *ptr = block->current;
        block->current += size;

        return ptr;
    }
    return NULL;
}

void *mdl_pool_alloc(mdl_pool_t *pool, size_t size)
{
    //mdl_pool_block_t *block = (mdl_pool_block_t *)pool->block_head_list.next;
    mdl_pool_block_t *block = list_entry(pool->block_head_list.next, mdl_pool_block_t, block_node_list);

    void *ptr = mdl_pool_alloc_from_block(block, size);
    if (!ptr)
    {
        ptr = mdl_pool_allocate_find(pool, size);
    }

    return ptr;
}

void *mdl_pool_zalloc(mdl_pool_t *pool, size_t size)
{
        return mdl_pool_calloc(pool, 1, size);
}



void *mdl_pool_calloc(mdl_pool_t *pool, size_t count, size_t size)
{
    void *buf = mdl_pool_alloc(pool, size * count);
    if (buf)
    {
        bzero(buf, size * count);
    }

    return buf;
}

const char *mdl_pool_getobjname(const mdl_pool_t *pool)
{
    return pool->name;
}

mdl_pool_t *mdl_pool_create(mdl_pool_factory_t *f, 
        const char *name,
        size_t initial_size, 
        size_t increment_size,
        mdl_pool_callback callback)
{
    return (*f->create_pool)(f, name, initial_size, increment_size, callback);
}

void mdl_pool_release(mdl_pool_t *pool)
{
    if (pool->factory->release_pool)
    {
        (*pool->factory->release_pool)(pool->factory, pool);
    }
}

