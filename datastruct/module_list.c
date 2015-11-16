#include "module_list.h"

void *module_list_push(module_list_t *l)
{
    void             *elt;
    module_list_part_t  *last;

    last = l->last;

    if (last->nelts == l->nalloc) {

        /* the last part is full, allocate a new list part */
        if (!(last = module_palloc(l->pool, sizeof(module_list_part_t)))) {
            return NULL;
        }

        if (!(last->elts = module_palloc(l->pool, l->nalloc * l->size))) {
            return NULL;
        }

        last->nelts = 0;
        last->next = NULL;

        l->last->next = last;
        l->last = last;
    }

    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;

}
