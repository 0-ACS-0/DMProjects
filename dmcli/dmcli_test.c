#include "inc/dmcli.h"

// Echo function!
void echo_fn(cmds_data_pt cdata){
   printf("\n");
   for (size_t i = 1; i < cdata->argc; i++){
      printf("%s ", cdata->argv[i]);
   }
   printf("\n\n");

}

int main(int argc, char ** argv){
   dmcli_t cli = {0};
   dmcli_init(&cli);
   dmcli_conf_prompt(&cli, "$> ");
   dmcli_add_cmd(&cli, "echo", "Return the argument/s issued after the command.", echo_fn);
   dmcli_loop(&cli);
   return 0;
}
