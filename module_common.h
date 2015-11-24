#ifndef _MODULE_COMMON_H_
#define _MODULE_COMMON_H_


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>

typedef unsigned int u_int;
typedef unsigned char u_char;




#define MODULE_OK 0
#define MODULE_ERROR -1
#define MODULE_INVALID_FILE -1
#define LF '\n'

#endif /* _MODULE_COMMON_H_ */
