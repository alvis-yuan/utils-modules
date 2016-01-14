#ifndef _INCLUDE_LOG_H_
#define _INCLUDE_LOG_H_


typedef unsigned int color_t;
typedef struct mdl_log_s mdl_log_t;
typedef void (*backend_write_log_func)(int level, const char *data, int len);

#define LOG_MAX_SIZE    4000
#define LOG_USE_STACK_BUFFER 1

struct mdl_log_s
{
    int     level;

    int     fd;

    unsigned    decorate;

    int     indent:1;

    int     color_enable:1;

    color_t color;

    backend_write_log_func  func;
};


/* log level */
enum log_level
{
    MDL_LOG_FATAL = 0,
    MDL_LOG_ERR = 1,
    MDL_LOG_WARN = 2,
    MDL_LOG_INFO = 3,
    MDL_LOG_DEBUG = 4,
    MDL_LOG_TRACE = 5,
    MDL_LOG_DETAIL = 6
};
#define MDL_LOG_MAX_LEVEL MDL_LOG_DETAIL
void mdl_log_set_level(int level);
int mdl_log_get_level(void);


/* log decoration */
enum log_decoration
{
    LOG_HAS_DAY_NAME   = 0x0001, /**< Include day name [default: no]         */
    LOG_HAS_YEAR       = 0x0002, /**< Include year digit [no]            */
    LOG_HAS_MONTH      = 0x0004, /**< Include month [no]             */
    LOG_HAS_DAY_OF_MON = 0x0008, /**< Include day of month [no]          */
    LOG_HAS_TIME       = 0x0010, /**< Include time [yes]             */
    LOG_HAS_MICRO_SEC  = 0x0020, /**< Include microseconds [yes]             */
    LOG_HAS_SENDER     = 0x0040, /**< Include sender in the log [yes]        */
    LOG_HAS_NEWLINE    = 0x0080, /**< Terminate each call with newline [yes] */
    LOG_HAS_PROCESS_ID  = 0x0100, /**< Include process identification [yes]     */
    LOG_HAS_SPACE      = 0x0200, /**< Include two spaces before log [yes]    */
    LOG_HAS_COLOR      = 0x0400, /**< Colorize logs [no]       */
    LOG_HAS_LEVEL_TEXT = 0x0800, /**< Include level text string [no]         */
    LOG_HAS_THREAD_ID  = 0x1000, /**< Include thread identification [no]     */
    LOG_HAS_THREAD_SWC = 0x2000, /**< Add mark when thread has switched [no]*/
    LOG_HAS_INDENT     = 0x4000 /**< Indentation. Say yes! [yes]            */

};
void mdl_log_set_decor(unsigned decor);
unsigned mdl_log_get_decor(void);



/* log indent */
#define MDL_LOG_INDENT_CHAR     '.'
#define MDL_LOG_INDENT_SIZE     1
#define LOG_MAX_INDENT      80
#define MDL_LOG_ENABLE_INDENT      1

void mdl_log_add_indent(int indent);
void mdl_log_push_indent(void);
void mdl_log_pop_indent(void);


/* color */
void mdl_log_set_color(int level, color_t color);
color_t mdl_log_get_color(int level);
enum {
    TERM_COLOR_B = 1,    /**< Blue.          */
    TERM_COLOR_R = 2,    /**< Red            */
    TERM_COLOR_G = 4,    /**< Green          */
    TERM_COLOR_BRIGHT = 8    /**< Bright mask.   */
};
#define TERM_HAS_COLOR 1




/* backend worker */
void mdl_log_set_log_func(backend_write_log_func func);
backend_write_log_func mdl_log_get_log_func(void);



/* external interfaces */
extern int log_max_level;
void log_core(int level, const char *format, ...);
#define log(level, format, ...) \
do \
{ \
    if (level <= log_max_level) \
    { \
        log_core(level, format, ##__VA_ARGS__); \
    } \
    else \
    { \
        \
    } \
}while(0)

int mdl_log_init(void);

#endif /* _INCLUDE_LOG_H_ */
