#include "module_log.h"

static const char *err_levels[] = {
    "stderr", "emerg", "alert", "crit", "error",
    "warn", "notice", "info", "debug"
};

static const char *debug_levels[] = {
    "debug_core", "debug_alloc", "debug_mutex", "debug_event",
    "debug_http", "debug_imap"
};

static module_log_t        module_log;
static module_open_file_t  module_stderr;

static void module_log_write(module_log_t *log, char *errstr, size_t len);

static int fill_time(char *buf)
{
    time_t now;
    struct tm *tm;

    time(&now);
    tm = localtime(&now);
    tm->tm_year += 1900;
    tm->tm_mon += 1;
    
    return snprintf(buf, sizeof("1970-01-01 00:00:00"),
            "%4d/%02d/%02d %02d:%02d:%02d",
            tm->tm_year, tm->tm_mon,
            tm->tm_mday, tm->tm_hour,
            tm->tm_min, tm->tm_sec);
}


/***********************************/
module_open_file_t *module_conf_open_file(module_cycle_t *cycle, module_str_t *name)
{
    module_str_t         full;
    u_int        i;
    module_list_part_t  *part;
    module_open_file_t  *file;


    if (name) {
        full = *name;


        part = &cycle->open_files.part;
        file = part->elts;

        for (i = 0; /* void */ ; i++) {

            if (i >= part->nelts) {
                if (part->next == NULL) {
                    break;
                }
                part = part->next;
                file = part->elts;
                i = 0;
            }

            if (full.len != file[i].name.len) {
                continue;
            }

            if (strcmp(full.data, file[i].name.data) == 0) {
                return &file[i];
            }
        }
    }

    if (!(file = module_list_push(&cycle->open_files))) {
        return NULL;
    }

    if (name) {
        file->fd = MODULE_INVALID_FILE;
        file->name = full;

    } else {
        file->fd = module_stderr_fileno;
        file->name.len = 0;
        file->name.data = NULL;
    }

    return file;
}

/***********************************/


void module_log_error_core(u_int level, module_log_t *log, int err,
        const char *fmt, ...)
{
    char      errstr[MAX_ERROR_STR];
    size_t    len, max;
    va_list   args;

    if (log->file->fd == MODULE_INVALID_FILE) {
        return;
    }

    len = fill_time(errstr);

    max = MAX_ERROR_STR - 1;


    len += snprintf(errstr + len, max - len, " [%s] ", err_levels[level]);

    /* pid#tid */
    len += snprintf(errstr + len, max - len,
            "%d#%ld: ", getpid(), syscall(SYS_gettid));

    if (log->data && *(int *) log->data != -1) {
        len += snprintf(errstr + len, max - len,
                "*%u ", *(u_int *) log->data);
    }


    va_start(args, fmt);
    len += vsnprintf(errstr + len, max - len, fmt, args);
    va_end(args);


    if (err) {

        if (len > max - 50) {

            /* leave a space for an error code */

            len = max - 50;
            errstr[len++] = '.';
            errstr[len++] = '.';
            errstr[len++] = '.';
        }

        len += snprintf(errstr + len, max - len, " (%d: ", err);

        if (len >= max) {
            module_log_write(log, errstr, max);
            return;
        }

        //len += module_strerror_r(err, errstr + len, max - len);
        len += snprintf(errstr + len, max - len, "%s", strerror(err));

        if (len >= max) {
            module_log_write(log, errstr, max);
            return;
        }

        errstr[len++] = ')';

        if (len >= max) {
            module_log_write(log, errstr, max);
            return;
        }

    } else {
        if (len >= max) {
            module_log_write(log, errstr, max);
            return;
        }
    }

    if (level != MODULE_LOG_DEBUG && log->handler) {
        len += log->handler(log->data, errstr + len, max - len);

        if (len >= max) {
            len = max;
        }
    }

    module_log_write(log, errstr, len);
}


static void module_log_write(module_log_t *log, char *errstr, size_t len)
{
    errstr[len++] = LF;
    write(log->file->fd, errstr, len);

}


module_log_t *module_log_init_stderr()
{
    module_log.file = &module_stderr;
    module_log.log_level = MODULE_LOG_ERR;

    return &module_log;
}


module_log_t *module_log_create_errlog(module_cycle_t *cycle, module_array_t *args)
{
    module_log_t  *log;
    module_str_t  *value, *name;

    if (args) {
        value = args->elts;
        name = &value[1];

    } else {
        name = NULL;
    }

    if (!(log = module_pcalloc(cycle->pool, sizeof(module_log_t)))) {
        return NULL;
    }

    if (!(log->file->name = name)) {
        return NULL;
    }

    return log;
}


int module_set_error_log_levels(module_conf_t *cf, module_log_t *log)
{
    u_int   i, n, d;
    module_str_t   *value;

    value = cf->args->elts;

    for (i = 2; i < cf->args->nelts; i++) {

        for (n = 1; n <= MODULE_LOG_DEBUG; n++) {
            if (module_strcmp(value[i].data, err_levels[n]) == 0) {

                if (log->log_level != 0) {
                    module_conf_log_error(MODULE_LOG_EMERG, cf, 0,
                            "invalid log level \"%s\"",
                            value[i].data);
                    return MODULE_ERROR;
                }

                log->log_level = n;
                continue;
            }
        }

        for (n = 0, d = MODULE_LOG_DEBUG_FIRST; d <= MODULE_LOG_DEBUG_LAST; d <<= 1) {
            if (module_strcmp(value[i].data, debug_levels[n++]) == 0) {
                if (log->log_level & ~MODULE_LOG_DEBUG_ALL) {
                    module_conf_log_error(MODULE_LOG_EMERG, cf, 0,
                            "invalid log level \"%s\"",
                            value[i].data);
                    return MODULE_ERROR;
                }

                log->log_level |= d;
            }
        }


        if (log->log_level == 0) {
            module_conf_log_error(MODULE_LOG_EMERG, cf, 0,
                    "invalid log level \"%s\"", value[i].data);
            return MODULE_ERROR;
        }
    }

    if (log->log_level == MODULE_LOG_DEBUG) {
        log->log_level = MODULE_LOG_DEBUG_ALL;
    }

    return MODULE_OK;
}

