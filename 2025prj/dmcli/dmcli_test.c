#include "inc/dmcli.h"

// Echo function!
void echo_fn(cmds_data_pt cdata){
   printf("\n");
   for (size_t i = 1; i < cdata->argc; i++){
      printf("%s ", cdata->argv[i]);
   }
   printf("\n\n");
}

// Sub-cli function!
void subcli_fn(cmds_data_pt cdata){
   // Super cli and subcli declarations:
   dmcli_pt supcli = cdata->exdata;
   dmcli_t subcli = {0};

   // Sub- prefix for the subclient prompt:
   char prompt[DEFAULT_IO_PROMPT_CAP] = {0};
   strncat(prompt, "sub-", 5);
   strncat(prompt, dmcli_io_get_prompt(&supcli->io), DEFAULT_IO_PROMPT_CAP-5);
   prompt[DEFAULT_IO_PROMPT_CAP - 1] = '\0';

   // Initialization, configuration and loop of subcli:
   dmcli_init(&subcli);
   dmcli_io_set_prompt(&subcli.io, prompt);
   dmcli_cmd_set_command(&subcli.cmd, "echo", "Return the argument/s issued after the command.", echo_fn);
   dmcli_cmd_set_command(&subcli.cmd, "subcli", "Starts a new client nested inside the previous subcli.", subcli_fn);
   dmcli_loop(&subcli);
}

/* ---- MAIN ---- */
int main(int argc, char ** argv){
   // Declaration and initialization of a cli:
   dmcli_t cli = {0};
   dmcli_init(&cli);

   // Configuration:
   dmcli_io_set_prompt(&cli.io, "cli $> ");

   // Adding commands to cli:
   dmcli_cmd_set_command(&cli.cmd, "echo", "Return the argument/s issued after the command.", echo_fn);
   dmcli_cmd_set_command(&cli.cmd, "subcli", "Starts a new client nested inside the previous subcli.", subcli_fn);
   
   // Client loop:
   dmcli_loop(&cli);
   return 0;
}
