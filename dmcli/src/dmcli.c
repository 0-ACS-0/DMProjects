/*

*/

/* ---- Libraries ------------------------------------------------- */
#include "../inc/dmcli.h"

/* ---- Helper functions implementation prototypes ---------------- */
void _dmcli_helper_helpcmd(cmds_data_pt cdata);
void _dmcli_helper_exitcmd(cmds_data_pt cdata);

/* ---- INTERNAL - Functions implementation ----------------------- */
// ======== Init:
/*
    @brief Function to initialize dmcli to be ready for its use, setting all to defaults.

    @param dmcli_pt dmcli: Reference to dmcli structure.

    @retval true: If initialization succeeded.
    @retval false: If initialization failed.
*/
bool dmcli_init(dmcli_pt dmcli){
    // Reference check:
    if (!dmcli) return false;

    // Return value:
    bool ret = true;

    // Initialization to 0's and defaults for input/output:
    dmcli->io = (dmcli_io_t){0};
    ret &= dmcli_io_set_default(&dmcli->io);

    // Initialization to 0's and defaults for commands:
    dmcli->cmd = (dmcli_cmd_t){0};
    ret &= dmcli_cmd_set_default(&dmcli->cmd);
    ret &= dmcli_cmd_set_exdata(&dmcli->cmd, dmcli);
    ret &= dmcli_cmd_set_command(&dmcli->cmd, "help", "Muestra todos los comandos disponibles con sus respectivas descripciones.", _dmcli_helper_helpcmd);
    ret &= dmcli_cmd_set_command(&dmcli->cmd, "exit", "Finaliza la interfaz por línea de comandos.", _dmcli_helper_exitcmd);

    return ret;
}

// ======== Loop:
/*
    @brief Function that implements the command line interface loop for input/output and commands.

    @param dmcli_pt dmcli: Reference to dmcli structure.
*/
void dmcli_loop(dmcli_pt dmcli){
    // Reference check:
    if (!dmcli) return;

    // Program loop:
    dmcli->is_running = true;
    dmcli_io_enterm_rawmode(&dmcli->io);
    while (dmcli->is_running){
        // Wait for user input:
        dmcli_io_wait4input(&dmcli->io);

        // Execute command if available:
        dmcli_cmd_execute(&dmcli->cmd, dmcli->io.input);
    }
    dmcli_io_disterm_rawmode(&dmcli->io);
}

// ======== CMD Add:
/*
    @brief Wrapper function to add a command to the command line interface.

    @param dmcli_pt dmcli: Reference to dmcli structure.
    @param const char * cmd_name: Command name string.
    @param const char * cmd_desc: Command description string.
    @param void (*cmd_fn)(void *): Command function reference.

    @retval true: If add succeeded.
    @retval false: If add failed.
*/
bool dmcli_add_cmd(dmcli_pt dmcli, const char * cmd_name, const char * cmd_desc, void (*cmd_fn)(cmds_data_pt)){
    // Reference check:
    if (!dmcli) return false;

    // Add the new command:
    bool ret = true;
    ret &= dmcli_cmd_set_command(&dmcli->cmd, cmd_name, cmd_desc, cmd_fn);
    return ret;
}

// ======== Settings:
bool dmcli_conf_prompt(dmcli_pt dmcli, const char * prompt_str){
    // References check:
    if (!dmcli || !prompt_str) return false;

    // Set the new prompt for the cli:
    bool ret = true;
    ret &= dmcli_io_set_prompt(&dmcli->io, prompt_str);
    return ret;
}

/* ---- INTERNAL - Helper functions implementation ----------------------- */
/*
    @brief Helper function that adds functionality to show a simple help output with all
    the commands available and their corresponding description.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.
    @param void * data: Extra data available. 
*/
void _dmcli_helper_helpcmd(cmds_data_pt cdata){
    // Reference check:
    if (!cdata) return;
    dmcli_pt cli = cdata->exdata;

    // Print of commands and descriptions:
    printf("\n================ HELP \n");
    for (size_t i = 0; i < cli->cmd.cmds_length; i++){
        printf("'%s': %s", cli->cmd.cmds[i], cli->cmd.cmds_descs[i]);
        printf("\n");
    }
    printf("\n");
}

/*
    @brief Helper function that ends the command line interface loop!


    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.
    @param void * data: Extra data available. 
*/
void _dmcli_helper_exitcmd(cmds_data_pt cdata){
    // Reference check:
    if (!cdata) return;
    dmcli_pt cli = cdata->exdata;

    // End the loop:
    cli->is_running = false;
}