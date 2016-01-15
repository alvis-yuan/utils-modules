#ifndef _MODULE_ARRAY_H_
#define _MODULE_ARRAY_H_

#include "../module_common.h"
#include "../pool/pool.h"


typedef struct mdl_array_s mdl_array_t;
struct mdl_array_s 
{
    void        *elts;

    uint   nelts;
    
    size_t       size;

    uint   nalloc;

    mdl_pool_t  *pool;
};


mdl_array_t *mdl_create_array(mdl_pool_t *p, uint n, size_t size);
void mdl_destroy_array(mdl_array_t *a);
void *mdl_push_array(mdl_array_t *a);


inline static int mdl_array_init(mdl_array_t *array, mdl_pool_t *pool,
        uint n, size_t size)
{
    if (!(array->elts = mdl_pool_alloc(pool, n * size))) 
    {
        return FAILURE;
    }

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    return SUCCESS;
}



#define mdl_init_array(a, p, n, s, rc)                                       \
    mdl_test_null(a.elts, mdl_palloc(p, n * s), rc);                         \
a.nelts = 0; a.size = s; a.nalloc = n; a.pool = p;

#define mdl_array_create  mdl_create_array
#define mdl_array_push    mdl_push_array


#endif /* _MODULE_ARRAY_H_*/
