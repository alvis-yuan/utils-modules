#include "log.h"
#include "../module_common.h"
#include "../thread/thread.h"

static long thread_suspended_tls_id = -1;
static long thread_indent_tls_id = -1;


int log_max_level = MDL_LOG_MAX_LEVEL;

static unsigned log_decor = LOG_HAS_TIME | LOG_HAS_MICRO_SEC |
LOG_HAS_NEWLINE |
LOG_HAS_SPACE | LOG_HAS_THREAD_ID |  
LOG_HAS_INDENT | LOG_HAS_PROCESS_ID;

void log_write(int level, const char *buffer, int len);
backend_write_log_func log_writer = &log_write;

static color_t LOG_COLOR_0 = TERM_COLOR_BRIGHT | TERM_COLOR_R;
static color_t LOG_COLOR_1 = TERM_COLOR_BRIGHT | TERM_COLOR_R;
static color_t LOG_COLOR_2 = TERM_COLOR_BRIGHT | 
TERM_COLOR_R | 
TERM_COLOR_G;
static color_t LOG_COLOR_3 = TERM_COLOR_BRIGHT | 
TERM_COLOR_R | 
TERM_COLOR_G | 
TERM_COLOR_B;
static color_t LOG_COLOR_4 = TERM_COLOR_R | 
TERM_COLOR_G | 
TERM_COLOR_B;
static color_t LOG_COLOR_5 = TERM_COLOR_R | 
TERM_COLOR_G | 
TERM_COLOR_B;
static color_t LOG_COLOR_6 = TERM_COLOR_R | 
TERM_COLOR_G | 
TERM_COLOR_B;
/* Default terminal color */
static color_t LOG_COLOR_77 = TERM_COLOR_R | 
TERM_COLOR_G | 
TERM_COLOR_B;


#if LOG_USE_STACK_BUFFER == 0
static char log_buffer[LOG_MAX_SIZE];
#endif

static void *g_last_thread;


/*******************************************************************************/
/* tools routines */
typedef struct parsed_time_s parsed_time_t;
struct parsed_time_s
{
    int wday;

    /** This represents day of month: 1-31 */
    int day;

    /** This represents month, with the value is 1 - 12  */
    int mon;

    /** This represent the actual year (unlike in ANSI libc where
     *  the value must be added by 1900).
     */
    int year;

    /** This represents the second part, with the value is 0-59 */
    int sec;

    /** This represents the minute part, with the value is: 0-59 */
    int min;

    /** This represents the hour part, with the value is 0-23 */
    int hour;

    /** This represents the milisecond part, with the value is 0-999 */
    int msec;

};



static int time_decode(const struct timeval *tv, parsed_time_t *pt)
{
    struct tm *local_time;

    local_time = localtime((time_t*)&tv->tv_sec);

    pt->year = local_time->tm_year+1900;
    pt->mon = local_time->tm_mon + 1;
    pt->day = local_time->tm_mday;
    pt->hour = local_time->tm_hour;
    pt->min = local_time->tm_min;
    pt->sec = local_time->tm_sec;
    pt->wday = local_time->tm_wday;
    pt->msec = tv->tv_usec/1000;

    return 0;
}

/*******************************************************************************/
/* level set/get */
void mdl_log_set_level(int level)
{
    log_max_level = level;
}

int mdl_log_get_level(void)
{
    return log_max_level;
}


/*******************************************************************************/

/* decoration get/set */
void mdl_log_set_decor(unsigned decor)
{
    log_decor = decor;
}

unsigned mdl_log_get_decor(void)
{
    return log_decor;
}


/*******************************************************************************/
/* indent operations */
static void log_set_indent(int indent)
{
        if (indent < 0)
        {
            indent = 0;
        }
        mdl_thread_local_set(thread_indent_tls_id, (void*)(ssize_t)indent);
}

static int log_get_raw_indent(void)
{
        return (long)(ssize_t)mdl_thread_local_get(thread_indent_tls_id);
}


static int log_get_indent(void)
{
    int indent = log_get_raw_indent();
    return indent > LOG_MAX_INDENT ? LOG_MAX_INDENT : indent;
}


void mdl_log_add_indent(int indent)
{
    log_set_indent(log_get_raw_indent() + indent);
}

void mdl_log_push_indent(void)
{
    mdl_log_add_indent(MDL_LOG_INDENT_SIZE);
}

void mdl_log_pop_indent(void)
{
    mdl_log_add_indent(-MDL_LOG_INDENT_SIZE);
}

/*******************************************************************************/

/* color */
void mdl_log_set_color(int level, color_t color)
{
    switch (level) 
    {
        case 0: LOG_COLOR_0 = color; 
                break;
        case 1: LOG_COLOR_1 = color; 
                break;
        case 2: LOG_COLOR_2 = color; 
                break;
        case 3: LOG_COLOR_3 = color; 
                break;
        case 4: LOG_COLOR_4 = color; 
                break;
        case 5: LOG_COLOR_5 = color; 
                break;
        case 6: LOG_COLOR_6 = color; 
                break;
                /* Default terminal color */
        case 77: LOG_COLOR_77 = color; 
                 break;
        default:
                 /* Do nothing */
                 break;
    }
}

color_t mdl_log_get_color(int level)
{
    switch (level) {
        case 0:
            return LOG_COLOR_0;
        case 1:
            return LOG_COLOR_1;
        case 2:
            return LOG_COLOR_2;
        case 3:
            return LOG_COLOR_3;
        case 4:
            return LOG_COLOR_4;
        case 5:
            return LOG_COLOR_5;
        case 6:
            return LOG_COLOR_6;
        default:
            /* Return default terminal color */
            return LOG_COLOR_77;
    }
}


/*******************************************************************************/
/* backend worker */
void mdl_log_set_log_func(backend_write_log_func func)
{
    log_writer = func;
}

backend_write_log_func mdl_log_get_log_func(void)
{
    return log_writer;
}


static int term_set_color(color_t color)
{
    /* put bright prefix to ansi_color */
    char ansi_color[12] = "\033[01;3";

    if (color & TERM_COLOR_BRIGHT) 
    {
        color ^= TERM_COLOR_BRIGHT;
    }
    else 
    {
        strcpy(ansi_color, "\033[00;3");
    }

    switch (color) {
        case 0:
            /* black color */
            strcat(ansi_color, "0m");
            break;
        case TERM_COLOR_R:
            /* red color */
            strcat(ansi_color, "1m");
            break;
        case TERM_COLOR_G:
            /* green color */
            strcat(ansi_color, "2m");
            break;
        case TERM_COLOR_B:
            /* blue color */
            strcat(ansi_color, "4m");
            break;
        case TERM_COLOR_R | TERM_COLOR_G:
            /* yellow color */
            strcat(ansi_color, "3m");
            break;
        case TERM_COLOR_R | TERM_COLOR_B:
            /* magenta color */
            strcat(ansi_color, "5m");
            break;
        case TERM_COLOR_G | TERM_COLOR_B:
            /* cyan color */
            strcat(ansi_color, "6m");
            break;
        case TERM_COLOR_R | TERM_COLOR_G | TERM_COLOR_B:
            /* white color */
            strcat(ansi_color, "7m");
            break;
        default:
            /* default console color */
            strcpy(ansi_color, "\033[00m");
            break;
    }

    fputs(ansi_color, stdout);

    return 0;
}


static void log_term_set_color(int level)
{
#if TERM_HAS_COLOR
        term_set_color(mdl_log_get_color(level));
#else
        (void)level;
#endif
}

static void log_term_restore_color(void)
{
#if TERM_HAS_COLOR
        /* Set terminal to its default color */
        term_set_color(mdl_log_get_color(77));
#endif
}


void log_write(int level, const char *buffer, int len)
{
    (void)len;

    if (mdl_log_get_decor() & LOG_HAS_COLOR) 
    {
        log_term_set_color(level);
        printf("%s", buffer);
        log_term_restore_color();
    }
    else 
    {
        printf("%s", buffer);
    }
}





/*******************************************************************************/
/* main interfaces */

#if HAS_THREADS
static void logging_shutdown(void)
{
    if (thread_suspended_tls_id != -1) 
    {
        mdl_thread_local_free(thread_suspended_tls_id);
        thread_suspended_tls_id = -1;
    }
#if MDL_LOG_ENABLE_INDENT
    if (thread_indent_tls_id != -1)
    {
        mdl_thread_local_free(thread_indent_tls_id);
        thread_indent_tls_id = -1;
    }
#endif
}
#endif  /* HAS_THREADS */


int mdl_log_init(void)
{
#if HAS_THREADS
    if (thread_suspended_tls_id == -1) 
    {
        int status;
        status = mdl_thread_local_alloc(&thread_suspended_tls_id);
        if (status != 0)
        {
            return status;
        }

#if MDL_LOG_ENABLE_INDENT
        status = mdl_thread_local_alloc(&thread_indent_tls_id);
        if (status != 0)
        {
            mdl_thread_local_free(thread_suspended_tls_id);
            thread_suspended_tls_id = -1;

            return status;
        }
#endif
        atexit(&logging_shutdown);
    }
#endif
    g_last_thread = NULL;

    return 0;
}


static void suspend_logging(int *saved_level)
{
    *saved_level = log_max_level;

#if HAS_THREADS
    if (thread_suspended_tls_id != -1) 
    {
        mdl_thread_local_set(thread_suspended_tls_id, 
                (void*)1);
    } 
    else
#endif
    {
        log_max_level = 0;
    }
}



static void resume_logging(int *saved_level)
{
#if HAS_THREADS
    if (thread_suspended_tls_id != -1) 
    {
        mdl_thread_local_set(thread_suspended_tls_id,
                (void*)0);
    }
    else
#endif
    {
        if (log_max_level == 0 && *saved_level)
        {
            log_max_level = *saved_level;
        }
    }
}


static int is_logging_suspended(void)
{
#if HAS_THREADS
    if (thread_suspended_tls_id != -1) 
    {
        return mdl_thread_local_get(thread_suspended_tls_id) != NULL;
    }
    else
#endif
    {
        return log_max_level == 0;
    }
}



void log_core(int level, const char *format, ...)
{
    struct timeval now;
    parsed_time_t ptime;
    int max;
    va_list args;
#if LOG_USE_STACK_BUFFER
    char log_buffer[LOG_MAX_SIZE] = {0};
#endif
    int saved_level, len, indent;

    if (level > log_max_level)
    {
        return;
    }

    if (is_logging_suspended())
    {
        return;
    }

    suspend_logging(&saved_level);

    /* Get current date/time. */
    gettimeofday(&now, NULL);
    time_decode(&now, &ptime);

    len = 0;
    max = LOG_MAX_SIZE - 1;

    /* level */
    if (log_decor & LOG_HAS_LEVEL_TEXT) 
    {
        static const char *ltexts[] = 
        { 
            "FATAL:", 
            "ERROR:", 
            " WARN:", 
            " INFO:",
            "DEBUG:", 
            "TRACE:", 
            "DETRC:"
        };
        len += snprintf(log_buffer, max - len, "%s", ltexts[level]);
    }


    /* weekday */
    if (log_decor & LOG_HAS_DAY_NAME) 
    {
        static const char *wdays[] =
        {
            "Sun", 
            "Mon",
            "Tue", 
            "Wed",
            "Thu",
            "Fri", 
            "Sat"
        };
        len += snprintf(log_buffer + len, max - len, "%s", wdays[ptime.wday]);
    }

    /* year */
    if (log_decor & LOG_HAS_YEAR) 
    {
        if (len)
        {
            len += snprintf(log_buffer + len, max - len , " %d", ptime.year);
        }
        else
        {
            len += snprintf(log_buffer + len, max - len , "%d", ptime.year);
        }
    }

    /* month */
    if (log_decor & LOG_HAS_MONTH) 
    {
        len += snprintf(log_buffer + len, max - len, "-%02d", ptime.mon);
    }

    /* day */
    if (log_decor & LOG_HAS_DAY_OF_MON) 
    {
        len += snprintf(log_buffer + len, max - len, "-%02d", ptime.day);
    }

    /* hour:min:sec */
    if (log_decor & LOG_HAS_TIME) 
    {
        len += snprintf(log_buffer + len, max - len, " %02d:%02d:%02d",
                ptime.hour, ptime.min, ptime.sec);
    }

    /* milliseconds */
    if (log_decor & LOG_HAS_MICRO_SEC) 
    {
        len += snprintf(log_buffer + len, max - len, ".%03d ", ptime.msec);
    }

    /* source */
    if (log_decor & LOG_HAS_SENDER) 
    {
        len += snprintf(log_buffer + len, max - len, " [%14s:%16s#%03d]",
                __FILE__, __func__, __LINE__);
    }

    /* pid */
    if (log_decor & LOG_HAS_THREAD_ID) 
    {
        len += snprintf(log_buffer + len, max - len, " [%8ld]", syscall(SYS_gettid));
    }

    /* thread id */
    if (log_decor & LOG_HAS_PROCESS_ID) 
    {
        len += snprintf(log_buffer + len, max - len, "#[%8d]",
                getpid());
    }

    if (log_decor != 0 && log_decor != LOG_HAS_NEWLINE)
    {
        len += snprintf(log_buffer + len, max - len, " ");
    }

    if (log_decor & LOG_HAS_THREAD_SWC)
    {
        //todo
    } 
    else if (log_decor & LOG_HAS_SPACE) 
    {
        len += snprintf(log_buffer + len, max - len, " ");
    }

#if MDL_LOG_ENABLE_INDENT
    if (log_decor & LOG_HAS_INDENT) 
    {
        indent = log_get_indent();
        if (indent > 0) 
        {
            memset(log_buffer + len, MDL_LOG_INDENT_CHAR, indent);
            len += indent;
        }
    }
#endif

    /* Print the whole message to the string log_buffer. */
    va_start(args, format);
    len += vsnprintf(log_buffer + len, max - len, format, args);
    va_end(args);

    if (len >= max)
    {
        goto out;
    }

    if (log_decor & LOG_HAS_NEWLINE) 
    {
        log_buffer[len++] = '\n';
    }

out:
    log_buffer[LOG_MAX_SIZE - 1] = '\0';
    resume_logging(&saved_level);

    if (log_writer)
        (*log_writer)(level, log_buffer, len);
}











