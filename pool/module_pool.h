#ifndef _MODULE_POOL_H_
#define _MODULE_POOL_H_

#include "../module_common.h"


#define MODULE_MAX_ALLOC_FROM_POOL 4095
#define MODULE_ALIGN       (sizeof(unsigned long) - 1)  /* platform word */
#define MODULE_ALIGN_CAST  (unsigned long)              /* size of the pointer */
#define module_align(p)    (char *) ((MODULE_ALIGN_CAST p + MODULE_ALIGN) & ~MODULE_ALIGN)

#define module_test_null(p, alloc, rc)  if ((p = alloc) == NULL) { return rc; }

typedef struct module_pool_large_s  module_pool_large_t;
typedef struct module_pool_s module_pool_t;

struct module_pool_large_s {
    module_pool_large_t  *next;
    void              *alloc;
};


struct module_pool_s {
    char              *last;
    char              *end;
    module_pool_t        *next;
    module_pool_large_t  *large;
    module_log_t         *log;
};

void *module_alloc(size_t size, module_log_t *log);
void *module_calloc(size_t size, module_log_t *log);

module_pool_t *module_create_pool(size_t size, module_log_t *log);
void module_destroy_pool(module_pool_t *pool);

void *module_palloc(module_pool_t *pool, size_t size);
void *module_pcalloc(module_pool_t *pool, size_t size);
int module_pfree(module_pool_t *pool, void *p);





#endif /* _MODULE_POOL_H_ */
