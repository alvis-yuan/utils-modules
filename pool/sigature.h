#ifndef _SIGNATURE_H_
#define _SIGNATURE_H_ 

#include <unistd.h>
#include <string.h>
#include <assert.h>

#define SAFE_POOL 0

#if SAFE_POOL

#define SIG_SIZE     sizeof(unsigned int)

static void apply_signature(void *p, size_t size);
static void check_pool_signature(void *p, size_t size);

#define APPLY_SIG(p,sz)  apply_signature(p,sz), \
                    p=(void*)(((char*)p)+SIG_SIZE)
#define REMOVE_SIG(p,sz) check_pool_signature(p,sz), \
                    p=(void*)(((char*)p)-SIG_SIZE)

#define SIG_BEGIN        0x600DC0DE
#define SIG_END      0x0BADC0DE

static void apply_signature(void *p, size_t size)
{
    unsigned sig;

    sig = SIG_BEGIN;
    memcpy(p, &sig, SIG_SIZE);

    sig = SIG_END;
    memcpy(((char*)p) + SIG_SIZE + size, &sig, SIG_SIZE);
}

static void check_pool_signature(void *p, size_t size)
{
        unsigned sig;
        unsigned char *mem = (unsigned char *)p;

        /* Check that signature at the start of the block is still intact */
        sig = SIG_BEGIN;
        assert(!memcmp(mem - SIG_SIZE, &sig, SIG_SIZE));

        sig = SIG_END;
        assert(!memcmp(mem + size, &sig, SIG_SIZE));
}

#else

#define SIG_SIZE     0
#define APPLY_SIG(p,sz)
#define REMOVE_SIG(p,sz)

#endif



#endif /* _SIGNATURE_H_ */
