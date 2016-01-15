#ifndef _MODULE_COMMON_H_
#define _MODULE_COMMON_H_


#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <semaphore.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/time.h>

typedef unsigned long       ulong;
typedef unsigned int        uint;
typedef unsigned short   ushort;
typedef unsigned char    uchar;

/* boolean */
typedef int bool;

/* file description */
typedef int fd_t;

/* enumeration value type */
typedef int status_t;

typedef long tid_t;


#define SUCCESS 0
#define  FAILURE (-1)
#define TRUE    1
#define FALSE   0


/* object name length */
#define MAX_OBJ_NAME    32


/* array length */
#define ARRAY_SIZE(a)    (sizeof(a)/sizeof(a[0]))


#define UNUSED_ARG(arg) (void)arg



/* assertion */
#define ASSERT_RETURN(condition, ret) \
    do\
{\
    if (!(condition))\
    {\
        fprintf(stderr, #condition" assertion failed in file [%s], function [%s], line [%d]\n", __FILE__, __func__, __LINE__);\
        return (ret);\
    }\
}while(0)


#define ASSERT_NORETURN(condition, ret)\
    do\
{\
    if (!(condition))\
    {\
        fprintf(stderr, #condition" assertion failed in file [%s], function [%s], line [%d]\n", __FILE__, __func__, __LINE__);\
    }\
}while(0)

#define CHECK_RETURN(condition, success, fail) \
    do\
{\
    if ((condition))\
    {\
        return success;\
    }\
    else\
    {\
        return fail;\
    }\
}while(0)

#define ENSURE_RETURN(condition, ret) \
    do \
{ \
    if (!(condition)) \
    { \
        return (ret); \
    } \
}while(0)


/* redefine library routines */
#define gettimeofcurrent(tp)    gettimeofday(tp, NULL)




/* time operations */

/* struct timeval ---> ms */
#define TIME_VAL_MSEC(t) ((t).tv_sec * 1000 + (t).tv_usec/1000)

/* equal */
#define TIME_VAL_EQ(t1, t2)  ((t1).tv_sec==(t2).tv_sec \
        && (t1).tv_usec==(t2).tv_usec)


/* greater */
#define TIME_VAL_GT(t1, t2)  ((t1).tv_sec>(t2).tv_sec || \
        ((t1).tv_sec==(t2).tv_sec && (t1).tv_usec>(t2).tv_usec))


/* greater or equal */
#define TIME_VAL_GTE(t1, t2) (TIME_VAL_GT(t1,t2) || \
        TIME_VAL_EQ(t1,t2))


/* less */
#define TIME_VAL_LT(t1, t2)  (!(TIME_VAL_GTE(t1,t2)))

/* less or equal */
#define TIME_VAL_LTE(t1, t2) (!TIME_VAL_GT(t1, t2))


/* two struct timeval add */
#define TIME_VAL_ADD(t1, t2) \
    do {                \
        (t1).tv_sec += (t2).tv_sec;       \
        (t1).tv_usec += (t2).tv_usec;     \
    } while (0)


/* two struct timeval minus */
#define TIME_VAL_SUB(t1, t2)  \
    do {                \
        if ((t1).tv_usec < (t2).tv_usec) \
        {\
            (t1).tv_sec--;\
            (t1).tv_usec += 1000000;\
        }\
        (t1).tv_sec -= (t2).tv_sec;       \
        (t1).tv_usec -= (t2).tv_usec;     \
    } while (0)
/* end of time operations */





















#endif /* _MODULE_COMMON_H_ */
