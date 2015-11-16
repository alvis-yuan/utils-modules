#ifndef _MODULE_LIST_H_
#define _MODULE_LIST_H_

#include "../module_common.h"

typedef struct module_list_part_s  module_list_part_t;

struct module_list_part_s {
    void             *elts;
    u_int        nelts;
    module_list_part_t  *next;
};


typedef struct {
    module_list_part_t  *last;
    module_list_part_t   part;
    size_t            size;
    u_int        nalloc;
    module_pool_t       *pool;
} module_list_t;


inline static int module_list_init(module_list_t *list, module_pool_t *pool,
        u_int n, size_t size)
{
    if (!(list->part.elts = module_palloc(pool, n * size))) {
        return MODULE_ERROR;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return MODULE_OK;
}

void *module_list_push(ngx_list_t *list);

#endif /* _MODULE_LIST_H_ */
