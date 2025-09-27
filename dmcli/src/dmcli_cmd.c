/*

*/

/* ---- Libraries ------------------------------------------------- */
#include "../inc/dmcli_cmd.h"
#include <stdbool.h>
#include <string.h>

/* ---- Helper functions implementation prototypes ---------------- */

/* ---- INTERNAL - Functions implementation ----------------------- */
// ======== Allocators:
/*
    @brief Function to allocate all the memory and prepare dmcli_cmd to be used with default
    configuration.
    @note For errors check dmcli_cmd pointer.

    @param dmcli_cmd_pt * dmcli_cmd: Reference to dmcli_cmd struct pointer.
*/
void dmcli_cmd_alloc(dmcli_cmd_pt * dmcli_cmd){
    // Reference check:
    if (!dmcli_cmd) return;

    // Structure allocation:
    *dmcli_cmd = calloc(1, sizeof(dmcli_cmd_t));
    if (!(*dmcli_cmd)) return;

    // Allocation of defaults:
    if (!dmcli_cmd_set_default(*dmcli_cmd)){
        free(*dmcli_cmd);
        *dmcli_cmd = NULL;
    }
}

/*
    @brief Function to deallocate all the memory reserved for dmcli_cmd structure.

    @param dmcli_cmd_pt * dmcli_cmd: Reference to dmcli_cmd struct pointer.
*/
void dmcli_cmd_dealloc(dmcli_cmd_pt * dmcli_cmd){
    // Reference check:
    if (!dmcli_cmd || !(*dmcli_cmd)) return;

    // Deallocation of all the commands names:
    for (size_t i = 0; i < (*dmcli_cmd)->cmds_capacity; i++){
        if ((*dmcli_cmd)->cmds && (*dmcli_cmd)->cmds[i]) free((*dmcli_cmd)->cmds[i]);
        if ((*dmcli_cmd)->cmds_descs && (*dmcli_cmd)->cmds_descs[i]) free((*dmcli_cmd)->cmds_descs[i]);
    }
    if ((*dmcli_cmd)->cmds) free((*dmcli_cmd)->cmds);
    if ((*dmcli_cmd)->cmds_descs) free((*dmcli_cmd)->cmds_descs);

    // Deallocation of the function pointer array:
    if ((*dmcli_cmd)->cmds_fn) free((*dmcli_cmd)->cmds_fn);

    // Global deallocation:
    free(*dmcli_cmd);
    *dmcli_cmd = NULL;
}


// ======== "Setters" & "Getters":
/*
    @brief Function to set the default configuration of dmcli_cmd.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.

    @retval true: If set succeeded.
    @retval false: If set failed.
*/
bool dmcli_cmd_set_default(dmcli_cmd_pt dmcli_cmd){
    // Reference check:
    if (!dmcli_cmd) return false;

    // Default calls:
    bool ret = true;
    ret &= dmcli_cmd_set_cap(dmcli_cmd, DEFAULT_CMD_CAP);

    return ret;
}

/*
    @brief Function to set the commands capacity of the structure, allocating it's memory.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.

    @retval true: If set succeeded.
    @retval false: If set failed.
*/
bool dmcli_cmd_set_cap(dmcli_cmd_pt dmcli_cmd, size_t capacity){
    // Reference check:
    if (!dmcli_cmd || (capacity == 0)) return false;

    // Allocate all the memory:
    dmcli_cmd->cmds = calloc(capacity, sizeof(char *));
    if (!dmcli_cmd->cmds) return false;
    dmcli_cmd->cmds_descs = calloc(capacity, sizeof(char *));
    if (!dmcli_cmd->cmds_descs) {free(dmcli_cmd->cmds); return false;}
    dmcli_cmd->cmds_fn = calloc(capacity, sizeof(void (*)(void *)));
    if (!dmcli_cmd->cmds_fn) {free(dmcli_cmd->cmds_descs); free(dmcli_cmd->cmds); return false;}

    // Set capacity and length:
    dmcli_cmd->cmds_capacity = capacity;
    dmcli_cmd->cmds_length = 0;
    return true;
}

/*
    @brief Function to set the custom user data reference into the command structure.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.
    @param void * udata_ref: User data reference (no type specification needed).

    @retval true: If set succeeded.
    @retval false: If set failed.
*/
bool dmcli_cmd_set_udata(dmcli_cmd_pt dmcli_cmd, void * udata_ref){
    // References check:
    if (!dmcli_cmd || !udata_ref) return false;

    // Reference copy:
    dmcli_cmd->udata = udata_ref;
    return true;
}

/*
    @brief Function to set a command name, description, and associated command function to execute when called.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.
    @param const char * cmd_name: Command name string.
    @param const char * cmd_desc: Command description string.
    @param void (*cmd_fn)(void *): Command function reference.

    @retval true: If set succeeded.
    @retval false: If set failed.
*/
bool dmcli_cmd_set_command(dmcli_cmd_pt dmcli_cmd, const char * cmd_name, const char * cmd_desc, void (*cmd_fn)(void *)){
    // References & bounds check:
    if (!dmcli_cmd || !cmd_name || !cmd_desc || !cmd_fn) return false;
    if (dmcli_cmd->cmds_length + 1 > dmcli_cmd->cmds_capacity) return false;

    // Copy the command name and description into memory:
    dmcli_cmd->cmds[dmcli_cmd->cmds_length] = strdup(cmd_name);
    if (!dmcli_cmd->cmds[dmcli_cmd->cmds_length]) return false;
    dmcli_cmd->cmds_descs[dmcli_cmd->cmds_length] = strdup(cmd_desc);
    if (!dmcli_cmd->cmds_descs[dmcli_cmd->cmds_length]) {free(dmcli_cmd->cmds[dmcli_cmd->cmds_length]); dmcli_cmd->cmds[dmcli_cmd->cmds_length] = NULL; return false;}

    // Copy the function reference of the command:
    dmcli_cmd->cmds_fn[dmcli_cmd->cmds_length] = cmd_fn;

    // Update command length:
    dmcli_cmd->cmds_length++;
    return true;
}

/*
    @brief Function to retrieve the capacity of commands of the structure.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.

    @return size_t: The total capacity.
*/
size_t dmcli_cmd_get_cap(dmcli_cmd_pt dmcli_cmd){
    // Reference check:
    if (!dmcli_cmd) return 0;

    // Return capacity:
    return dmcli_cmd->cmds_capacity;
}

/*
    @brief Function to retrieve the length of commands of the structure.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.

    @return size_t: The total length.   
*/
size_t dmcli_cmd_get_len(dmcli_cmd_pt dmcli_cmd){
    // Reference check:
    if (!dmcli_cmd) return 0;

    // Return length:
    return dmcli_cmd->cmds_length;
}

/*
    @brief Function to retreive the reference of the user data.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.

    @return void *: Reference of the user data.
*/
void * dmcli_cmd_get_udata(dmcli_cmd_pt dmcli_cmd){
    // Reference check:
    if (!dmcli_cmd) return NULL;

    // Return reference:
    return dmcli_cmd->udata;
}

// ======== Utils:
/*
    @brief Function that search for a command and if it finds it, execute the corresponding
    command function.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.
    @param const char * cmd: Command to execute.

    @retval true: If execution succeeded.
    @retval false: If execution failed.
*/
bool dmcli_cmd_execute(dmcli_cmd_pt dmcli_cmd, const char * cmd){
    // Reference checks:
    if (!dmcli_cmd || !cmd) return false;

    // Linear search loop to find the command issued:
    ssize_t cmd_index = -1;
    for (size_t i = 0; i < dmcli_cmd->cmds_length; i++){
        if (!strcmp(dmcli_cmd->cmds[i], cmd)) {cmd_index = i; break;}
    }

    // Command not found case:
    if (cmd_index == -1) return false;

    // Command function execution:
    if (dmcli_cmd->cmds_fn[cmd_index]) dmcli_cmd->cmds_fn[cmd_index](dmcli_cmd->udata);
    return true;
}