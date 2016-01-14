#include "../thread/thread.h"
#include "../pool/pool.h"


void *func(void *arg)
{
    UNUSED_ARG(arg);
    while(1);
}

int main(int argc, char **argv)
{
    int i;

    UNUSED_ARG(argc);
    UNUSED_ARG(argv);

    mdl_thread_t *threadpool[10] = {NULL};

    mdl_caching_pool_t caching_pool;
    mdl_pool_t  *pool;

    mdl_caching_pool_init(&caching_pool, NULL, 0);
    pool = mdl_pool_create(&caching_pool.factory, "testpool", 1024, 1024, NULL);

    mdl_thread_init();

    for (i = 0; i < 10; ++i)
    {
        int ret = mdl_thread_create(pool, NULL, func, NULL, 0, THREAD_SUSPENDED, &threadpool[i]);
        if (ret != SUCCESS)
        {
            printf("============fail==============\n");
        }
    }

    mdl_thread_dump();

    return 0;
}
