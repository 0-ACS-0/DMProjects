/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMCLI_CMD_HEADER
#define _DMCLI_CMD_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "dmcli_hdrs.h"

/* ---- Defines & macros ------------------------------------------ */
#define DEFAULT_CMD_CAP 200

/* ---- Data structures ------------------------------------------- */
// Simple dmcli conf structure (used externally for customized configuration functions only):
struct dmcli_cmd_conf{
};

// Simple dmcli command structure:
struct dmcli_cmd{
    char ** cmds;           // Array to all the commands strings available.
    char ** cmds_descs;     // Array to all the commands strings descriptions.
    void (**cmds_fn) (void *);   // Array to functions corresponding to the commands available.
    void * udata;           // Custom user data to be used inside command functions.

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

/* ---- Functions prototypes -------------------------------------- */
// Allocatr / Deallocator:
void dmcli_cmd_alloc(dmcli_cmd_pt * dmcli_cmd);
void dmcli_cmd_dealloc(dmcli_cmd_pt * dmcli_cmd);

// "Setters" / "Getters":
bool dmcli_cmd_set_default(dmcli_cmd_pt dmcli_cmd);
bool dmcli_cmd_set_cap(dmcli_cmd_pt dmcli_cmd, size_t capacity);
bool dmcli_cmd_set_udata(dmcli_cmd_pt dmcli_cmd, void * udata_ref);
bool dmcli_cmd_set_command(dmcli_cmd_pt dmcli_cmd, const char * cmd_name, const char * cmd_desc, void (*cmd_fn)(void *));

size_t dmcli_cmd_get_cap(dmcli_cmd_pt dmcli_cmd);
size_t dmcli_cmd_get_len(dmcli_cmd_pt dmcli_cmd); 
void * dmcli_cmd_get_udata(dmcli_cmd_pt dmcli_cmd);

// Utils:
bool dmcli_cmd_execute(dmcli_cmd_pt dmcli_cmd, const char * cmd);

#endif