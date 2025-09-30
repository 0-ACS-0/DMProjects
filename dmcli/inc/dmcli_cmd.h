/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMCLI_CMD_HEADER
#define _DMCLI_CMD_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "dmcli_hdrs.h"

/* ---- Defines & macros ------------------------------------------ */
#define DEFAULT_CMD_CAP 200
#define DEFAULT_CMD_NAME_LEN 128
#define DEFAULT_CMD_DESC_LEN 1024
#define DEFAULT_CMD_ARGV_CAP 16
#define DEFAULT_CMD_ARGV_LEN 64

/* ---- Data structures ------------------------------------------- */
// Simple dmcli conf structure (used externally for customized configuration functions only):
struct dmcli_cmd_conf{
};

// Custom user data to be used inside command functions.
struct cmds_data{
    void * exdata;
    void * udata;
    int argc;
    char argv[DEFAULT_CMD_ARGV_LEN][DEFAULT_CMD_ARGV_LEN];
};

// Simple dmcli command structure:
struct dmcli_cmd{
    char cmds[DEFAULT_CMD_CAP][DEFAULT_CMD_NAME_LEN];           // Array to all the commands strings available.
    char cmds_descs[DEFAULT_CMD_CAP][DEFAULT_CMD_DESC_LEN];     // Array to all the commands strings descriptions.
    void (*cmds_fn[DEFAULT_CMD_CAP]) (struct cmds_data *);   // Array to functions corresponding to the commands available.

    struct cmds_data cdata; // Commands data.

    size_t cmds_length;     // Commands lenght (number of commands in aplication).
    size_t cmds_capacity;   // Commands capacity (maximum number of commands in aplication).
};

/* ---- Data types ------------------------------------------------ */
// Configuration type:
typedef struct dmcli_cmd_conf dmcli_cmd_conf_t;
typedef dmcli_cmd_conf_t * dmcli_cmd_conf_pt;

// Simple dmcli cmd data type:
typedef struct dmcli_cmd dmcli_cmd_t;
typedef dmcli_cmd_t * dmcli_cmd_pt;

// Command data data type:
typedef struct cmds_data cmds_data_t;
typedef cmds_data_t * cmds_data_pt;

/* ---- Functions prototypes -------------------------------------- */
// "Setters" / "Getters":
bool dmcli_cmd_set_default(dmcli_cmd_pt dmcli_cmd);
bool dmcli_cmd_set_udata(dmcli_cmd_pt dmcli_cmd, void * udata_ref);
bool dmcli_cmd_set_exdata(dmcli_cmd_pt dmcli_cmd, void * exdata_ref);

bool dmcli_cmd_set_command(dmcli_cmd_pt dmcli_cmd, const char * cmd_name, const char * cmd_desc, void (*cmd_fn)(cmds_data_pt));

size_t dmcli_cmd_get_cap(dmcli_cmd_pt dmcli_cmd);
size_t dmcli_cmd_get_len(dmcli_cmd_pt dmcli_cmd); 
void * dmcli_cmd_get_udata(dmcli_cmd_pt dmcli_cmd);
void * dmcli_cmd_get_exdata(dmcli_cmd_pt dmcli_cmd);
// Utils:
bool dmcli_cmd_execute(dmcli_cmd_pt dmcli_cmd, const char * cmd);

#endif