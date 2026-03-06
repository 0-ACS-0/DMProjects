/*

*/

/* ---- Libraries ------------------------------------------------- */
#include "../inc/dmcli_cmd.h"
#include "../inc/dmcli_io.h"
#include <string.h>

/* ---- INTERNAL - Functions implementation ----------------------- */
// ======== "Setters" & "Getters":
/*
    @brief Function to set default values to the members.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.

    @retval true: If set succeeded.
    @retval false: If set failed.
*/
bool dmcli_cmd_set_default(dmcli_cmd_pt dmcli_cmd){
    // Reference check:
    if (!dmcli_cmd) return false;

    // Set the commands capacity:
    dmcli_cmd->cmds_capacity = DEFAULT_CMD_CAP;

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
    dmcli_cmd->cdata.udata = udata_ref;
    return true;
}

/*
    @brief Function to set the custom extra data reference into the command structure.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.
    @param void * udata_ref: User data reference (no type specification needed).

    @retval true: If set succeeded.
    @retval false: If set failed.
*/
bool dmcli_cmd_set_exdata(dmcli_cmd_pt dmcli_cmd, void * exdata_ref){
    // References check:
    if (!dmcli_cmd || !exdata_ref) return false;

    // Reference copy:
    dmcli_cmd->cdata.exdata = exdata_ref;
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
bool dmcli_cmd_set_command(dmcli_cmd_pt dmcli_cmd, const char * cmd_name, const char * cmd_desc, void (*cmd_fn)(cmds_data_pt)){
    // References & bounds check:
    if (!dmcli_cmd || !cmd_name || !cmd_desc || !cmd_fn) return false;
    if (dmcli_cmd->cmds_length + 1 > dmcli_cmd->cmds_capacity) return false;
    if (strlen(cmd_name) >= DEFAULT_CMD_NAME_LEN) return false;
    if (strlen(cmd_desc) >= DEFAULT_CMD_DESC_LEN) return false;

    // Copy the command name and description into memory:
    strncpy(dmcli_cmd->cmds[dmcli_cmd->cmds_length], cmd_name, DEFAULT_CMD_NAME_LEN);
    strncpy(dmcli_cmd->cmds_descs[dmcli_cmd->cmds_length], cmd_desc, DEFAULT_CMD_DESC_LEN);

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
    return dmcli_cmd->cdata.udata;
}

/*
    @brief Function to retreive the reference of the extra data.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.

    @return void *: Reference of the user data.
*/
void * dmcli_cmd_get_exdata(dmcli_cmd_pt dmcli_cmd){
    // Reference check:
    if (!dmcli_cmd) return NULL;

    // Return reference:
    return dmcli_cmd->cdata.exdata;
}


// ======== Utils:
/*
    @brief Function that searches for a command and if it finds it, executes the corresponding
    command function.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.
    @param const char * cmd: Command to execute.

    @retval true: If execution succeeded.
    @retval false: If execution failed.
*/
bool dmcli_cmd_execute(dmcli_cmd_pt dmcli_cmd, const char * input){
    // Reference and empty input checks:
    if (!dmcli_cmd || !input) return false;
    if (input[0] == '\0') return false;

    // Copy input into local variable:
    char temp_input[DEFAULT_IO_INPUT_CAP];
    strncpy(temp_input, input, DEFAULT_IO_INPUT_CAP);
    temp_input[DEFAULT_IO_INPUT_CAP - 1] = '\0';

    // Reset the argv and argc members of cdata:
    memset(dmcli_cmd->cdata.argv, '\0', DEFAULT_CMD_ARGV_CAP * DEFAULT_CMD_ARGV_LEN);
    dmcli_cmd->cdata.argc = 0;

    // Count and save the input arguments and command:
    char * arg = strtok((char *)temp_input, " ");
    while (arg && (dmcli_cmd->cdata.argc < DEFAULT_CMD_ARGV_CAP)){
        if (strlen(arg) >= DEFAULT_CMD_ARGV_LEN) return false;
        strncpy(dmcli_cmd->cdata.argv[dmcli_cmd->cdata.argc], arg, DEFAULT_CMD_ARGV_LEN);
        dmcli_cmd->cdata.argv[dmcli_cmd->cdata.argc++][DEFAULT_CMD_ARGV_LEN - 1] = '\0';
        arg = strtok(NULL, " ");
    }

    // Linear search loop to find the command issued:
    ssize_t cmd_index = -1;
    for (size_t i = 0; i < dmcli_cmd->cmds_length; i++){
        if (!strcmp(dmcli_cmd->cmds[i], dmcli_cmd->cdata.argv[0])) {cmd_index = i; break;}
    }

    // Command not found case:
    if (cmd_index == -1) return false;

    // Command function execution:
    if (dmcli_cmd->cmds_fn[cmd_index]) dmcli_cmd->cmds_fn[cmd_index](&dmcli_cmd->cdata);
    return true;
}

