#include "dmserver.h"
#include <stdio.h>

// Global server variable:
dmserver_pt serv;

// Echo function that broadcast to all clients the data read from any client:
void echo_fn(void * args){
    // Reference check & cast:
    if (!args) return;
    struct dmserver_cliconn * cli = (struct dmserver_cliconn *)args;

    // Broadcast received data to all clients:
    dmserver_broadcast(serv, cli->crbuffer);
    memset(cli->crbuffer, 0, DEFAULT_CCONN_RBUFFERLEN);
}

// MAIN:
int main(int argc, char ** argv){
    // Server initialization:
    dmserver_init(&serv);
    if (serv == NULL) exit(1);

    // Server logger configuration:
    if(!dmlogger_conf_output_file(serv->slogger, "./logs/", "dmserver", true, true, 0)) exit(1);
    if(!dmlogger_conf_logger_minlvl(serv->slogger, DMLOGGER_LEVEL_DEBUG)) exit(1);
    if(!dmlogger_conf_queue_ofpolicy(serv->slogger, DMLOGGER_OFPOLICY_DROP, 0)) exit(1);

    // Server connection data configuration:
    if (!dmserver_conf_port(serv, 2020)) exit(1);
    if (!dmserver_conf_safamily(serv, AF_INET6)) exit(1);
    if (!dmserver_conf_ipv6only(serv, false)) exit(1);
    if (!dmserver_conf_certpath(serv, "./certs/server.crt")) exit(1);
    if (!dmserver_conf_keypath(serv, "./certs/server.key")) exit(1);

    // Server callbacks set:
    if (!dmserver_setcb_onclientrcv(serv, echo_fn)) exit(1);

    // Server open + run:
    if (!dmserver_open(serv)) exit(1);
    if (!dmserver_run(serv)) exit(1);

    // Simple program loop:
    char c[12] = "";
    while (strcmp(c, "exit()")){
        printf("Finish server with 'exit()' call>> ");
        fflush(stdout);
        fgets(c, 12, stdin);
        c[strlen(c) - 1] = '\0';
    }

    // Server stop + close:
    if (!dmserver_stop(serv)) exit(1);
    if (!dmserver_close(serv)) exit(1);

    // Server deinit:
    dmserver_deinit(&serv);
    if (serv != NULL) exit(1);

    return 0;
}

