#include "pool.h"
#include <stdio.h>
#include "../log/log.h"


int main(int argc, char **argv)
{
    UNUSED_ARG(argc);
    UNUSED_ARG(argv);

    mdl_caching_pool_t caching_pool;
    mdl_pool_t  *pool;
    int *p;

    //mdl_log_init();

    mdl_caching_pool_init(&caching_pool, NULL, 0);

    pool = mdl_pool_create(&caching_pool.factory, "testpool", 1024, 1024, NULL);

    p = mdl_pool_alloc(pool, 32);
    *p = 44;

    printf("%d\n", *p);

    caching_pool.factory.dump_status(&caching_pool.factory, 1);

    return 0;
}
