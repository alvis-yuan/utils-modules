#ifndef _POOL_INTERNAL_H
#define _POOL_INTERNAL_H

#include "../datastruct/list.h"
#include "../log/log.h"
#include "../module_common.h"

#define UNUSED_ARG(arg)    (void)arg
#define POOL_ALIGNMENT      4

#ifndef log
#define log(level, format, ...) \
    do \
{ \
    printf(format, ##__VA_ARGS__); \
    printf("\n");\
}while(0)
#endif

typedef struct mdl_pool_factory_s mdl_pool_factory_t;
typedef struct mdl_pool_s mdl_pool_t;
typedef struct mdl_pool_block_s mdl_pool_block_t;
typedef struct mdl_caching_pool_s mdl_caching_pool_t;
typedef struct mdl_pool_factory_policy_s mdl_pool_factory_policy_t;
typedef int (*mdl_pool_callback)(mdl_pool_t *pool, size_t size);


void *mdl_pool_allocate_find(mdl_pool_t *pool, size_t size);



/* base class inheritted by mdl_caching_pool_t */
struct mdl_pool_factory_policy_s
{
    void* (*block_alloc)(mdl_pool_factory_t *factory, size_t size);

    void (*block_free)(mdl_pool_factory_t *factory, void *mem, size_t size);

    mdl_pool_callback callback;

    unsigned flags;
};

/* policy-->factory-->caching_pool */
struct mdl_pool_factory_s
{
    mdl_pool_factory_policy_t policy;

    mdl_pool_t *(*create_pool)(mdl_pool_factory_t *factory,
            const char *name,
            size_t initial_size, 
            size_t increment_size,
            mdl_pool_callback callback);

    void (*release_pool)(mdl_pool_factory_t *factory, mdl_pool_t *pool);

    void (*dump_status)(mdl_pool_factory_t *factory, int detail);

    int (*on_block_alloc)(mdl_pool_factory_t *factory, size_t size);

    void (*on_block_free)(mdl_pool_factory_t *factory, size_t size);
};


struct mdl_pool_s
{
    /* node chained other pool into a list */
    struct list_head   pool_node_list;

#define LEN_POOL_NAME 32
    /*  name */
    char        name[LEN_POOL_NAME];

    size_t      capacity;

    /* size of memory block to expand when pool run out of memory */
    size_t      increment_size;

    /**/
    mdl_pool_factory_t   *factory;

    void        *data_factory;

    /* list head of memory block allocated from the pool */
    struct list_head    block_head_list;

    /* callback when the pool can't allocate memory */
    mdl_pool_callback    callback;
};


struct mdl_pool_block_s
{
    struct list_head    block_node_list;

    unsigned char       *start;
    unsigned char       *current;
    unsigned char       *end;
};

struct mdl_caching_pool_s
{
    mdl_pool_factory_t  factory;

    size_t              capacity;

    size_t              max_capacity;

    size_t              used_count;

    size_t              used_size;

    size_t              peak_used_size;

#define CACHING_POOL_ARRAY_SIZE  16
    struct list_head    free_list[CACHING_POOL_ARRAY_SIZE];

    /* using pool list head, node is pool->pool_node_list */
    struct list_head    used_list;

    char        pool_buf[256 * (sizeof(size_t) / 4)];

    pthread_mutex_t     lock;
};


size_t mdl_pool_get_capacity(mdl_pool_t *pool);

size_t mdl_pool_get_used_size(mdl_pool_t *pool);

void *mdl_pool_alloc_from_block(mdl_pool_block_t *block, size_t size);

void *mdl_pool_alloc(mdl_pool_t *pool, size_t size);

void *mdl_pool_calloc(mdl_pool_t *pool, size_t count, size_t size);


void *mdl_pool_zalloc(mdl_pool_t *pool, size_t size);

const char *mdl_pool_getobjname(const mdl_pool_t *pool);

mdl_pool_t *mdl_pool_create(mdl_pool_factory_t *f, 
        const char *name,
        size_t initial_size, 
        size_t increment_size,
        mdl_pool_callback callback);

void mdl_pool_release(mdl_pool_t *pool);


/* instance of pool factory: caching_pool */
void mdl_caching_pool_init(mdl_caching_pool_t *cp, 
        const mdl_pool_factory_policy_t *policy,
        size_t max_capacity);

void mdl_caching_pool_destroy(mdl_caching_pool_t *cp);

#endif /* _POOL_INTERNAL_H */
