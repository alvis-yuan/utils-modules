#ifndef _MODULE_ARRAY_H_
#define _MODULE_ARRAY_H_

#include "../module_common.c"


typedef struct module_array_s {
    void        *elts;
    u_int   nelts;
    size_t       size;
    u_int   nalloc;
    module_pool_t  *pool;
}module_array_t;


module_array_t *module_create_array(module_pool_t *p, u_int n, size_t size);
void module_destroy_array(module_array_t *a);
void *module_push_array(module_array_t *a);


inline static int module_array_init(module_array_t *array, module_pool_t *pool,
        u_int n, size_t size)
{
    if (!(array->elts = module_palloc(pool, n * size))) {
        return MODULE_ERROR;
    }

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    return MODULE_OK;
}



#define module_init_array(a, p, n, s, rc)                                       \
    module_test_null(a.elts, module_palloc(p, n * s), rc);                         \
a.nelts = 0; a.size = s; a.nalloc = n; a.pool = p;

#define module_array_create  module_create_array
#define module_array_push    module_push_array


#endif /* _MODULE_ARRAY_H_*/
