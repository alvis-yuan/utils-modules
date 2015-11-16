#ifndef _MODULE_STRING_H_
#define _MODULE_STRING_H_

#include "../module_common.h"


typedef struct {
    size_t    len;
    u_char   *data;
} module_str_t;


#define module_string(str)  { sizeof(str) - 1, (u_char *) str }
#define module_null_string  { 0, NULL }

u_char *module_cpystrn(u_char *dst, u_char *src, size_t n);
int module_rstrncmp(u_char *s1, u_char *s2, size_t n);



#endif /* _MODULE_STRING_H_ */
