#ifndef _MODULE_LOG_H_
#define _MODULE_LOG_H_

#include "../module_common.h"
#include "../datastruct/module_array.h"

#define MODULE_LOG_STDERR            0
#define MODULE_LOG_EMERG             1
#define MODULE_LOG_ALERT             2
#define MODULE_LOG_CRIT              3
#define MODULE_LOG_ERR               4
#define MODULE_LOG_WARN              5
#define MODULE_LOG_NOTICE            6
#define MODULE_LOG_INFO              7
#define MODULE_LOG_DEBUG             8

#define MODULE_LOG_DEBUG_CORE        0x010
#define MODULE_LOG_DEBUG_ALLOC       0x020
#define MODULE_LOG_DEBUG_MUTEX       0x040
#define MODULE_LOG_DEBUG_EVENT       0x080
#define MODULE_LOG_DEBUG_HTTP        0x100
#define MODULE_LOG_DEBUG_IMAP        0x200

/*
 *   do not forget to update debug_levels[] in src/core/module_log.c
 *   after the adding a new debug level
 */

#define MODULE_LOG_DEBUG_FIRST       MODULE_LOG_DEBUG_CORE
#define MODULE_LOG_DEBUG_LAST        MODULE_LOG_DEBUG_IMAP
#define MODULE_LOG_DEBUG_CONNECTION  0x80000000
#define MODULE_LOG_DEBUG_ALL         0x7ffffff0



typedef struct module_log_s {
    u_int           log_level;
    int             fd;
} module_log_t;

#define MAX_ERROR_STR    2048

#define module_log_error(level, log, args...) \
    if (log->log_level >= level) module_log_error_core(level, log, args)

void module_log_error_core(u_int level, module_log_t *log, int err,
        const char *fmt, ...);

#define module_log_debug0(level, log, err, fmt) \
    if (log->log_level & level) \
module_log_error_core(MODULE_LOG_DEBUG, log, err, fmt)

#define module_log_debug1(level, log, err, fmt, arg1) \
    if (log->log_level & level) \
module_log_error_core(MODULE_LOG_DEBUG, log, err, fmt, arg1)

#define module_log_debug2(level, log, err, fmt, arg1, arg2) \
    if (log->log_level & level) \
module_log_error_core(MODULE_LOG_DEBUG, log, err, fmt, arg1, arg2)

#define module_log_debug3(level, log, err, fmt, arg1, arg2, arg3) \
    if (log->log_level & level) \
module_log_error_core(MODULE_LOG_DEBUG, log, err, fmt, arg1, arg2, arg3)

#define module_log_debug4(level, log, err, fmt, arg1, arg2, arg3, arg4) \
    if (log->log_level & level) \
module_log_error_core(MODULE_LOG_DEBUG, log, err, fmt, arg1, arg2, arg3, arg4)

#define module_log_debug5(level, log, err, fmt, arg1, arg2, arg3, arg4, arg5) \
    if (log->log_level & level) \
module_log_error_core(MODULE_LOG_DEBUG, log, err, fmt, \
        arg1, arg2, arg3, arg4, arg5)

#define module_log_debug6(level, log, err, fmt, \
        arg1, arg2, arg3, arg4, arg5, arg6) \
if (log->log_level & level) \
module_log_error_core(MODULE_LOG_DEBUG, log, err, fmt, \
        arg1, arg2, arg3, arg4, arg5, arg6)

#define module_log_debug7(level, log, err, fmt, \
        arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
if (log->log_level & level) \
module_log_error_core(MODULE_LOG_DEBUG, log, err, fmt, \
        arg1, arg2, arg3, arg4, arg5, arg6, arg7)

#define module_log_alloc_log(pool, log)  module_palloc(pool, log, sizeof(module_log_t))
#define module_log_copy_log(new, old)    module_memcpy(new, old, sizeof(module_log_t))

module_log_t *module_log_init_stderr();
module_log_t *module_log_create_errlog(module_cycle_t *cycle, module_array_t *args);
int module_set_error_log_levels(module_conf_t *cf, module_log_t *log);



#endif /* _MODULE_LOG_H_ */
