#include "module_array.h"

module_array_t *module_create_array(module_pool_t *p, u_int n, size_t size)
{
    module_array_t *a;

    module_test_null(a, module_palloc(p, sizeof(module_array_t)), NULL);

    module_test_null(a->elts, module_palloc(p, n * size), NULL);

    a->pool = p;
    a->nelts = 0;
    a->nalloc = n;
    a->size = size;

    return a;
}


void module_destroy_array(module_array_t *a)
{
    module_pool_t  *p;

    p = a->pool;

    if ((char *) a->elts + a->size * a->nalloc == p->last) {
        p->last -= a->size * a->nalloc;
    }

    if ((char *) a + sizeof(module_array_t) == p->last) {
        p->last = (char *) a;
    }
}


void *module_push_array(module_array_t *a)
{
    void        *elt, *new;
    module_pool_t  *p;

    /* array is full */
    if (a->nelts == a->nalloc) {
        p = a->pool;

        /* array allocation is the last in the pool */
        if ((char *) a->elts + a->size * a->nelts == p->last
                && (unsigned) (p->end - p->last) >= a->size)
        {
            p->last += a->size;
            a->nalloc++;

            /* allocate new array */
        } else {
            module_test_null(new, module_palloc(p, 2 * a->nalloc * a->size), NULL);

            memcpy(new, a->elts, a->nalloc * a->size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }

    elt = (char *) a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}


