/*

*/

/* ---- Libraries ------------------------------------------------- */
#include "../inc/dmcli.h"

/* ---- Helper functions implementation prototypes ---------------- */
void _dmcli_helper_helpcmd(void * cdata);
void _dmcli_helper_exitcmd(void * cdata);

/* ---- INTERNAL - Functions implementation ----------------------- */
// ======== Init / Deinit:
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


/* ---- INTERNAL - Helper functions implementation ----------------------- */
/*
    @brief Helper function that adds functionality to show a simple help output with all
    the commands available and their corresponding description.

    @param dmcli_cmd_pt dmcli_cmd: Reference to dmcli_cmd structure.
    @param void * data: Extra data available. 
*/
void _dmcli_helper_helpcmd(void * cdata){
    // Reference check:
    if (!cdata) return;
    cmds_data_pt cmds_data = (cmds_data_pt)cdata;
    dmcli_pt cli = cmds_data->exdata;

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
void _dmcli_helper_exitcmd(void * cdata){
    // Reference check:
    if (!cdata) return;
    cmds_data_pt cmds_data = (cmds_data_pt)cdata;
    dmcli_pt cli = cmds_data->exdata;

    // End the loop:
    cli->is_running = false;
}