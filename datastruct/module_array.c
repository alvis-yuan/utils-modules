#include "module_array.h"

mdl_array_t *mdl_create_array(mdl_pool_t *p, uint n, size_t size)
{
    mdl_array_t *a;

    mdl_test_null(a, mdl_palloc(p, sizeof(mdl_array_t)), NULL);

    mdl_test_null(a->elts, mdl_palloc(p, n * size), NULL);

    a->pool = p;
    a->nelts = 0;
    a->nalloc = n;
    a->size = size;

    return a;
}


void mdl_destroy_array(mdl_array_t *a)
{
    mdl_pool_t  *p;

    p = a->pool;

    if ((char *) a->elts + a->size * a->nalloc == p->last) {
        p->last -= a->size * a->nalloc;
    }

    if ((char *) a + sizeof(mdl_array_t) == p->last) {
        p->last = (char *) a;
    }
}


void *mdl_push_array(mdl_array_t *a)
{
    void        *elt, *new;
    mdl_pool_t  *p;

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
            mdl_test_null(new, mdl_palloc(p, 2 * a->nalloc * a->size), NULL);

            memcpy(new, a->elts, a->nalloc * a->size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }

    elt = (char *) a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}


