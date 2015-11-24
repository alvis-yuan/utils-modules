#include "datastruct/module_string.h"
#include "pool/module_pool.h"
#include "log/module_log.h"
#include "datastruct/module_array.h"
#include "datastruct/module_list.h"

typedef struct module_open_file_s {
    int   fd;
    module_str_t  name;
}module_open_file_t;

typedef struct module_cycle_s {
    void           ****conf_ctx;
    module_pool_t        *pool;

    module_log_t         *log;
    module_log_t         *new_log;

    module_array_t        listening;
    module_array_t        pathes;
    module_list_t         open_files;

    //module_uint_t         connection_n;
    //module_connection_t  *connections;
    //module_event_t       *read_events;
    //module_event_t       *write_events;

    //module_cycle_t       *old_cycle;

    module_str_t          conf_file;
    module_str_t          root;
}module_cycle_t;



typedef struct module_conf_s {
    char                 *name;
    module_array_t          *args;

    module_cycle_t          *cycle;
    module_pool_t           *pool;
//    module_conf_file_t      *conf_file;
    module_log_t            *log;

    void                 *ctx;
    u_int            module_type;
    u_int            cmd_type;

    char                 *handler_conf;
}module_conf_t;
