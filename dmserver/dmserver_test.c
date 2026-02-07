#include "./inc/dmserver.h"

// ---- Global server variable:
dmserver_pt serv;

// ---- Callback functions prototypes:
void echo_fn(dmserver_cliconn_pt cli);
void wc_fn(dmserver_cliconn_pt cli);
void gb_fn(dmserver_cliconn_pt cli);
void to_fn(dmserver_cliconn_pt cli);

// ---- Main program:
int main(int argc, char ** argv){
    // Server initialization:
    dmserver_init(&serv);
    if (serv == NULL) exit(1);

    // Server logger configuration:
    if(!dmlogger_conf_output_file(serv->slogger, "./logs/", "dmserver", true, true, 0)) exit(1);
    if(!dmlogger_conf_logger_minlvl(serv->slogger, DMLOGGER_LEVEL_DEBUG)) exit(1);
    if(!dmlogger_conf_queue_ofpolicy(serv->slogger, DMLOGGER_OFPOLICY_DROP, 0)) exit(1);

    // Server connection data configuration:
    if (!dmserver_conf_sconn(serv, &(dmserver_servconn_conf_t){
        .sport=7890,
        .ssa_family=AF_INET,
        .sipv6_only=false,
        .stls_enable=true,
        .scert_path="./certs/server.crt",
        .skey_path="./certs/server.key"
    })) exit(1);
    
    // Worker configuration:
    if (!dmserver_conf_worker(serv, &(dmserver_worker_conf_t){
        .wth_subthreads=8,
        .wth_clispersth=200,
        .wth_clistimeout=120
    })) exit(1);

    // Client buffers configuration:
    if (!dmserver_conf_cconn(serv, &(dmserver_cliconn_conf_t){
        .cread_buffer_size = 1024,
        .cwrite_buffer_size = 1024
    })) exit(1);

    // Server callbacks set:
    if (!dmserver_set_cb(serv, &(dmserver_callback_conf_t){
        .on_client_timeout = to_fn,
        .on_client_disconnect = gb_fn,
        .on_client_connect = wc_fn,
        .on_client_rcv = echo_fn,
    })) exit(1);

    // Server open + run:
    if (!dmserver_open(serv)) exit(1);
    if (!dmserver_run(serv)) exit(1);

    // Simple program loop (cmd -> exit / broadcast / unicast / disconnect):
    char c[12] = "";
    char msg[4096] = "";
    while (strcmp(c, "exit")){
        // Prompt and user input read:
        printf("Finish server with 'exit' call>> ");
        fflush(stdout);
        fgets(c, 12, stdin);
        c[strlen(c) - 1] = '\0';

        // Broadcast command issued:
        if (!strcmp(c, "broadcast")){
            printf("> Enter message to broadcast: ");
            fflush(stdout);
            fgets(msg, 4096, stdin);
            msg[strlen(msg) - 1] = '\0';
            dmserver_broadcast(serv, NULL, msg);
            memset(msg, 0, 4096);

            printf("\n");
            fflush(stdout);
        }

        // Unicast command issued:
        if (!strcmp(c, "unicast")){
            int i[2] = {0};
            printf("> Client th_pos: ");
            scanf("%d", &i[0]);
            printf("> Client wc_pos: ");
            scanf("%d", &i[1]);
            getchar();

            printf("> Enter message to unicast: ");
            fflush(stdout);
            fgets(msg, 4096, stdin);
            msg[strlen(msg) - 1] = '\0';

            dmserver_unicast(serv, &(dmserver_cliloc_t){.th_pos=i[0], .wc_pos=i[1]}, msg);
            memset(msg, 0, 4096);

            printf("\n");
            fflush(stdout);
        }

        // Disconnect command issued:
        if (!strcmp(c, "disconnect")){
            int i[2] = {0};
            printf("> Client th_pos: ");
            scanf("%d", &i[0]);
            printf("> Client wc_pos: ");
            scanf("%d", &i[1]);
            getchar();
            dmserver_disconnect(serv, &(dmserver_cliloc_t){.th_pos=i[0], .wc_pos=i[1]});
    
            printf("\n");
            fflush(stdout);
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



// ---- Callback functions:
// Echo function that broadcast to all clients except the one being read:
void echo_fn(dmserver_cliconn_pt cli){
    // Reference check:
    if (!cli) return;   

    // Broadcast received data to all clients (chat mode):
    dmserver_broadcast(serv, &cli->cloc, cli->crbuffer);
}

// Welcome funciton that unicast to a newer connected client:
void wc_fn(dmserver_cliconn_pt cli){
    // Reference check:
    if (!cli) return;

    // Unicast welcome msg data to new client:
    char wc_msg[] = "Bienvenido a dmserver!\r\n";
    dmserver_unicast(serv, &cli->cloc, wc_msg);
}

// Goodbye function that broadcast the disconnected client to all connected clients:
void gb_fn(dmserver_cliconn_pt cli){
    // Reference check:
    if (!cli) return;

    // Broadcast goodbye msg data to connected clients:
    char gb_msg[] = "Un cliente se ha desconectad.\r\n";
    dmserver_broadcast(serv, NULL, gb_msg);
}

// Timeou function that broadcast the timedout client to all connected clients:
void to_fn(dmserver_cliconn_pt cli){
    // Reference check:
    if (!cli) return;

    // Broadcast timedout msg data to connected clients:
    char to_msg[] = "Un cliente ha sufrido un timeout.\r\n";
    dmserver_broadcast(serv, NULL, to_msg);
}

