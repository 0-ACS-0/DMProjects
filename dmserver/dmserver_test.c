#include "inc/dmserver.h"

// Global server variable:
dmserver_pt serv;

// Echo function that unicast to the same client the data read from himself:
void echo_fn(dmserver_cliconn_pt cli){
    // Reference check:
    if (!cli) return;

    // Broadcast received data to all clients:
    dmserver_unicast(serv, &cli->cloc, cli->crbuffer);
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
    if (!dmserver_conf_safamily(serv, AF_INET)) exit(1);
    if (!dmserver_conf_ipv6only(serv, false)) exit(1);
    if (!dmserver_conf_tlsenable(serv, true)) exit(1);
    if (!dmserver_conf_certpath(serv, "./certs/server.crt")) exit(1);
    if (!dmserver_conf_keypath(serv, "./certs/server.key")) exit(1);

    // Server callbacks set:
    if (!dmserver_setcb_onclientrcv(serv, echo_fn)) exit(1);

    // Server open + run:
    if (!dmserver_open(serv)) exit(1);
    if (!dmserver_run(serv)) exit(1);

    // Simple program loop (cmd -> exit() / broadcast / unicast):
    char c[12] = "";
    while (strcmp(c, "exit()")){
        // Prompt and user input read:
        printf("Finish server with 'exit()' call>> ");
        fflush(stdout);
        fgets(c, 12, stdin);
        c[strlen(c) - 1] = '\0';

        // Broadcast command issued:
        if (!strcmp(c, "broadcast")){
            dmserver_broadcast(serv, "Broadcast!\n");
        }

        // Unicast command issued:
        if (!strcmp(c, "unicast")){
            int i[2] = {0};
            printf("> Unicast th_pos: ");
            scanf("%d", &i[0]);
            printf("> Unicast wc_pos: ");
            scanf("%d", &i[1]);
            getchar();
            dmserver_unicast(serv, &(dmserver_cliloc_t){.th_pos=i[0], .wc_pos=i[1]}, "Unicast!\n");
        }

        // Disconnect command issued:
        if (!strcmp(c, "disconnect")){
            int i[2] = {0};
            printf("> Unicast th_pos: ");
            scanf("%d", &i[0]);
            printf("> Unicast wc_pos: ");
            scanf("%d", &i[1]);
            getchar();
            dmserver_disconnect(serv, &(dmserver_cliloc_t){.th_pos=i[0], .wc_pos=i[1]});
        }
    }

    // Server stop + close:
    if (!dmserver_stop(serv)) exit(1);
    if (!dmserver_close(serv)) exit(1);

    // Server deinit:
    dmserver_deinit(&serv);
    if (serv != NULL) exit(1);

    return 0;
}

