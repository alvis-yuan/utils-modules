files:
    policy.c: according to 'mdl_pool_factory_policy_t' class instantiate a object 'mdl_pool_factory_default_policy'. then need add it to pool.h for external use, and provide a routine 'mdl_pool_factory_get_default_policy' for get the object instance.need to add to pool.h:
        1. global variable 'mdl_pool_factory_default_policy'
        2. interface 'mdl_pool_factory_get_default_policy'
    
    signature.h: suffix and prefix a magic number to a requested memory area, for the sake of protect memory overstack. (need define macro SAFE_POOL)

    caching.c: class 'mdl_caching_pool_t' inherit base class 'mdl_pool_factory_t' then provide interfaces to instantiate its object. Below interfaces need to add pool.h for external use;
        1. mdl_caching_pool_init: instantiate object interface
        2. mdl_caching_pool_destroy: destroy object

    pool.c: pool operations, 
        1. create and release pool
        2. allocate memory from pool
        3. get attributes of pool

    pool.h: class abstract and interfaces for external use
