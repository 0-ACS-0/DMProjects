#include "inc/dmcli.h"
#include "inc/dmcli_cmd.h"
#include "inc/dmcli_io.h"

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
   dmcli_conf_prompt(&subcli, prompt);
   dmcli_add_cmd(&subcli, "echo", "Return the argument/s issued after the command.", echo_fn);
   dmcli_add_cmd(&subcli, "subcli", "Starts a new client nested inside the previous subcli.", subcli_fn);
   dmcli_loop(&subcli);
}

/* ---- MAIN ---- */
int main(int argc, char ** argv){
   // Declaration and initialization of a cli:
   dmcli_t cli = {0};
   dmcli_init(&cli);

   // Configuration:
   dmcli_conf_prompt(&cli, "cli $> ");

   // Adding commands to cli:
   dmcli_add_cmd(&cli, "echo", "Return the argument/s issued after the command.", echo_fn);
   dmcli_add_cmd(&cli, "subcli", "Starts a new client nested inside the previous subcli.", subcli_fn);
   
   // Client loop:
   dmcli_loop(&cli);
   return 0;
}
