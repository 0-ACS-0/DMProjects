/* ---- Library --------------------------------------------------- */
#include "../inc/dmserver.h"





/* ---- Function implementation ----------------------------------- */
// ======== Initialization / Deinitialization:
/*
    @brief Function to initialize a dmserver "object" to work with (memory management), 
    initialized to defaults.

    @param dmserver_pt * dmserver: Reference to external pointer to dmserver object.
    @note: (Check for null value in pointer reference in case of errors.)
*/
void dmserver_init(dmserver_pt * dmserver){
    // Check reference:
    if (!dmserver) return;

    // Dmserver alloc:
    (*dmserver) = calloc(1, sizeof(dmserver_t));
    if (!(*dmserver)) return;

    // Dmserver-logger initialization (and run) to defaults:
    dmlogger_init(&(*dmserver)->slogger);
    if (!(*dmserver)->slogger) {dmserver_deinit(dmserver); return;}
    if (!dmlogger_run((*dmserver)->slogger)) {dmserver_deinit(dmserver); return;}

    // Dmserver-sconn initialization to defaults:
    (*dmserver)->sconn.sfd = -1;
    (*dmserver)->sconn.sport = DEFAULT_SCONN_SPORT;
    (*dmserver)->sconn.sfamily = DEFAULT_SCONN_SFAMILY;
    strncpy((*dmserver)->sconn.sssl_certpath, DEFAULT_SCONN_CERTPATHVAL, DEFAULT_SCONN_CERTPATHLEN);
    (*dmserver)->sconn.sssl_certpath[DEFAULT_SCONN_CERTPATHLEN - 1] = '\0';
    strncpy((*dmserver)->sconn.sssl_keypath, DEFAULT_SCONN_KEYPATHVAL, DEFAULT_SCONN_KEYPATHLEN);
    (*dmserver)->sconn.sssl_keypath[DEFAULT_SCONN_KEYPATHLEN - 1] = '\0';

    // Dmserver-cconn (worker) initialization to defaults:
    (*dmserver)->sworker.wmainepfd = epoll_create1(0);
    if ((*dmserver)->sworker.wmainepfd == -1){dmserver_deinit(dmserver); return;}
    for (size_t i = 0; i < DEFAULT_WORKER_SUBTHREADS; i++){
        (*dmserver)->sworker.wsubepfd[i] = epoll_create1(0);
        if ((*dmserver)->sworker.wsubepfd[i] == -1){dmserver_deinit(dmserver); return;}
        for (size_t j = 0; j < DEFAULT_WORKER_CLISPERSTH; j++){
            if (!_dmserver_cconn_init(&(*dmserver)->sworker.wcclis[i][j])) {dmserver_deinit(dmserver); return;}
        }
    }
    (*dmserver)->sworker.wctimeout = DEFAULT_WORKER_CLITIMEOUT;
    
    // Ignore sigpipe signal to avoid SSL exceptions:
    signal(SIGPIPE, SIG_IGN);

    // Dmserver-state initialized:
    (*dmserver)->sstate = DMSERVER_STATE_INITIALIZED;
    dmlogger_log((*dmserver)->slogger, DMLOGGER_LEVEL_INFO, "-------- DMServer initialization completed at (%p).\n", (*dmserver));
}

/*
    @brief Function to deinitialize a dmserver "object" (memory management).

    @param dmserver_pt * dmserver: Reference to external pointer to dmserver object.
*/
void dmserver_deinit(dmserver_pt * dmserver){
    // Check external reference and dmserver reference:
    if (!dmserver || !(*dmserver)) return;

    // Check server state when deinitializing:
    if (((*dmserver)->sstate != DMSERVER_STATE_INITIALIZED) && ((*dmserver)->sstate != DMSERVER_STATE_CLOSED)) return;

    // Dmserver-cconn deinitialization:
    if ((*dmserver)->sworker.wmainepfd != -1) close((*dmserver)->sworker.wmainepfd);
    for (size_t i = 0; i < DEFAULT_WORKER_SUBTHREADS; i++){
        if ((*dmserver)->sworker.wsubepfd[i] != -1) close((*dmserver)->sworker.wsubepfd[i]);
        for (size_t j = 0; j < DEFAULT_WORKER_CLISPERSTH; j++){
            _dmserver_cconn_deinit(&(*dmserver)->sworker.wcclis[i][j]);
        }
    }

    // Dmserver-logger deinitialization (internally flush and dealloc):
    if ((*dmserver)->slogger) {dmlogger_log((*dmserver)->slogger, DMLOGGER_LEVEL_INFO, "-------- DMServer at (%p) deinitialized.\n", (*dmserver)); dmlogger_deinit(&(*dmserver)->slogger);}

    // Dmserver dealloc:
    free((*dmserver));
    (*dmserver) = NULL;
}




// ======== Open / Run / Stop / Close:
/*
    @brief Function to open the server, refering to initializing the server socket
    and having the server ready to start receiving, but not running.

    @param dmserver_pt * dmserver: Reference to dmserver struct.

    @retval true: Open succeeded.
    @retval false: Open failed.
*/
bool dmserver_open(dmserver_pt dmserver){
    // Reference check & state check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Initialize server connection data and ssl:
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "DMServer opening...");
    if (!_dmserver_sconn_init(&dmserver->sconn)) return false;
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_open() - server connection data initialized.");

    if (dmserver->sconn.sssl_enable){
        if (!_dmserver_sconn_sslinit(&dmserver->sconn)) {_dmserver_sconn_deinit(&dmserver->sconn); return false;}
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_open() - server ssl data initialized.");
    }

    // Start listening on server socket:
    if (!_dmserver_sconn_listen(&dmserver->sconn)) {_dmserver_sconn_ssldeinit(&dmserver->sconn); _dmserver_sconn_deinit(&dmserver->sconn); return false;}
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_open() - server listening, with a backlog of size %d.", SOMAXCONN);

    // Server state update:
    dmserver->sstate = DMSERVER_STATE_OPENED;

    char sip_str[INET6_ADDRSTRLEN];
    const void * addr = (dmserver->sconn.sfamily == AF_INET) ? (void*)&dmserver->sconn.saddr.s4.sin_addr : (void*)&dmserver->sconn.saddr.s6.sin6_addr;
    inet_ntop(dmserver->sconn.sfamily, addr, sip_str, sizeof(sip_str));
    int sport_num = dmserver->sconn.sport;
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "DMServer open with address %s:%d.\n", sip_str, sport_num);

    return true;
}

/*
    @brief Function to close the server.

    @param dmserver_pt * dmserver: Reference to dmserver struct.

    @retval true: Close succeeded.
    @retval false: Close failed.
*/
bool dmserver_close(dmserver_pt dmserver){
    // Reference & state check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_OPENED) && (dmserver->sstate != DMSERVER_STATE_STOPPED)) return false;

    // Close (in order) all connection data within the server:
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "DMServer closing...");
    _dmserver_sconn_deinit(&dmserver->sconn);
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_close() - server connection data deinitialized.");

    if (dmserver->sconn.sssl_enable) {
        _dmserver_sconn_ssldeinit(&dmserver->sconn);
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_close() - server ssl data deinitialized.");
    }

    // Server state update:
    dmserver->sstate = DMSERVER_STATE_CLOSED;
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "DMServer closed.\n");

    return true;
}

/*
    @brief Function to run the server threads and start managing connections.

    @param dmserver_pt * dmserver: Reference to dmserver struct.

    @retval true: Run succeeded.
    @retval false: Run failed.
*/
bool dmserver_run(dmserver_pt dmserver){
    // Reference & state check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_OPENED) && (dmserver->sstate != DMSERVER_STATE_STOPPED)) return false;

    // Previous state update (thread run depends on it):
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "DMServer starting run...");
    dmserver->sstate = DMSERVER_STATE_RUNNING;

    // Subordinate threads launch (args freed inside the subordinate thread):
    for (size_t i = 0; i < DEFAULT_WORKER_SUBTHREADS; i++){
        dmserver_subthargs_t * args = calloc(1, sizeof(dmserver_subthargs_t));
        if (!args) return false; 
        args->dmserver = dmserver; args->subthindex = i;
        if (pthread_create(&dmserver->sworker.wsubth[i], NULL, _dmserver_worker_sub, args)){
            free(args);
            dmserver_stop(dmserver);
            return false;
        }
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_run() - Launched subordinated thread %d.", dmserver->sworker.wsubth[i]);
    }

    // Main thread launch:
    if(pthread_create(&dmserver->sworker.wmainth, NULL, _dmserver_worker_main, dmserver)){
        dmserver_stop(dmserver);
        return false;
    };
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_run() - Launched main thread %d.", dmserver->sworker.wmainth);

    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "DMServer running.\n");
    return true;
}

/*
    @brief Function to stop the server threads and stop managing connections.

    @param dmserver_pt * dmserver: Reference to dmserver struct.

    @retval true: Stop succeeded.
    @retval false: Stop failed.
*/
bool dmserver_stop(dmserver_pt dmserver){
    // Reference & state check:
    if (!dmserver) return false;
    if (dmserver->sstate != DMSERVER_STATE_RUNNING) return false;


    // Previous state update (thread run depends on it):
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "DMServer stopping...");
    dmserver->sstate = DMSERVER_STATE_STOPPING;

    // Force threads to finish by signaling wthctl and block-wait:
    pthread_join(dmserver->sworker.wmainth, NULL);
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_stop() - Stopped main thread %d.", dmserver->sworker.wmainth);

    for (size_t i = 0; i < DEFAULT_WORKER_SUBTHREADS; i++){
        pthread_join(dmserver->sworker.wsubth[i], NULL);
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_stop() - Stopped subordinated thread %d.", dmserver->sworker.wsubth[i]);
    }

    // Update server state:
    dmserver->sstate = DMSERVER_STATE_STOPPED;
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "DMServer stopped.\n");

    return true;
}




// ======== Broadcast / Unicast / Disconnect:
/*
    @brief Function to broadcast data through all the connected clients.
    @note: This function only works if the server is running.
    @note: If an error happens when writting to a single client, that client will be 
    ignored.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param const char * bcdata: Pointer to broadcast data to sent.

    @retval false: Broadcast failed.
    @retval true: Broadcast succeeded.
*/
bool dmserver_broadcast(dmserver_pt dmserver, dmserver_cliloc_pt bexclude, const char * bcdata){
    // References & state check:
    if (!dmserver || !bcdata) return false;
    if (dmserver->sstate != DMSERVER_STATE_RUNNING) return false;
    if (strlen(bcdata) >= DEFAULT_CCONN_WBUFFERLEN) return false;

    // Broadcast write to every connected client:
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Starting broadcast...");
    for (size_t i = 0; i < DEFAULT_WORKER_SUBTHREADS; i++){for (size_t j = 0; j < DEFAULT_WORKER_CLISPERSTH; j++){
        // Check client broadcast condition:
        struct dmserver_cliconn * dmclient = &dmserver->sworker.wcclis[i][j];
        if (dmclient->cstate != DMSERVER_CLIENT_ESTABLISHED) continue;
        if (bexclude && (dmclient->cloc.th_pos == bexclude->th_pos) && (dmclient->cloc.wc_pos == bexclude->wc_pos)) continue;

        // Copy broadcast data to the client write buffer:
        pthread_mutex_lock(&dmclient->cwlock);

        strncpy(dmclient->cwbuffer, bcdata, DEFAULT_CCONN_WBUFFERLEN - 1);
        dmclient->cwbuffer[DEFAULT_CCONN_WBUFFERLEN - 1] = '\0';
        dmclient->cwlen = strlen(dmclient->cwbuffer);

        pthread_mutex_unlock(&dmclient->cwlock);

        // Enable the output event on his epoll file descriptor:
        if (epoll_ctl(dmserver->sworker.wsubepfd[i], EPOLL_CTL_MOD, dmclient->cfd, &(struct epoll_event){.events=EPOLLIN|EPOLLOUT|EPOLLET, .data.ptr=dmclient}) < 0) {
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "Broadcast not queued to client %d.", dmclient->cfd);
            continue;
        }
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "Broadcast queued to client %d.", dmclient->cfd);
    }}
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Broadcast finalized.\n");

    return true;
}

/*
    @brief Function to unicast data through the selected client.
    @note: This function only works if the server is running.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param dmserver_cliloc_pt dmcliloc: Client location coordenates.
    @param const char * ucdata: Pointer to unicast data to sent.

    @retval false: Unicast failed.
    @retval true: Unicast succeeded.
*/
bool dmserver_unicast(dmserver_pt dmserver, dmserver_cliloc_pt dmcliloc, const char * ucdata){
    // References & state check:
    if (!dmserver || !dmcliloc || !ucdata) return false;
    if (dmserver->sstate != DMSERVER_STATE_RUNNING) return false;
    if (strlen(ucdata) >= DEFAULT_CCONN_WBUFFERLEN) return false;

    // Client established check:
    struct dmserver_cliconn * dmclient = &dmserver->sworker.wcclis[dmcliloc->th_pos][dmcliloc->wc_pos];
    if (dmclient->cstate != DMSERVER_CLIENT_ESTABLISHED) return false;

    // Copy unicast data to the client write buffer:
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Starting unicast to client %d...", dmclient->cfd);
    pthread_mutex_lock(&dmclient->cwlock);

    strncpy(dmclient->cwbuffer, ucdata, DEFAULT_CCONN_WBUFFERLEN - 1);
    dmclient->cwbuffer[DEFAULT_CCONN_WBUFFERLEN - 1] = '\0';
    dmclient->cwlen = strlen(dmclient->cwbuffer);

    pthread_mutex_unlock(&dmclient->cwlock);

    // Enable the output event on his epoll file descriptor:
    if (epoll_ctl(dmserver->sworker.wsubepfd[dmcliloc->th_pos], EPOLL_CTL_MOD, dmclient->cfd, &(struct epoll_event){.events=EPOLLIN|EPOLLOUT|EPOLLET, .data.ptr=dmclient}) < 0) {
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "Unicast not queued.");
        return false;
    }
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "Unicast queued.");

    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Unicast finalized.\n");
    return true;
}   

/*
    @brief Function to force a client to disconnect from the server.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param dmserver_cliloc_pt dmcliloc: Client location coordenates.

    @retval true: Client disconnected correctly.
    @retval false: Client disconnection failed.
*/
bool dmserver_disconnect(dmserver_pt dmserver, dmserver_cliloc_pt dmcliloc){
    // References, & client state check:
    if (!dmserver) return false;
    dmserver_cliconn_pt cli = &dmserver->sworker.wcclis[dmcliloc->th_pos][dmcliloc->wc_pos];
    if ((cli->cstate != DMSERVER_CLIENT_ESTABLISHED) && (cli->cstate != DMSERVER_CLIENT_ESTABLISHING)) return false;

    // Client socket file descriptor deletion from epoll:
    epoll_ctl(dmserver->sworker.wsubepfd[dmcliloc->th_pos], EPOLL_CTL_DEL, cli->cfd, NULL);

    // Disconnection proccess:
    if (dmserver->sconn.sssl_enable){SSL_shutdown(cli->cssl); SSL_free(cli->cssl);}
    close(cli->cfd);

    // Client state to closed:
    cli->cstate = DMSERVER_CLIENT_CLOSED;
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Disconnected client %d.\n", cli->cfd);

    // Client structure reset:
    _dmserver_cconn_reset(cli);
    dmserver->sworker.wccount[dmcliloc->th_pos]--;

    // User specific data processing of disconnected client:
    if (dmserver->scallback.on_client_disconnect) dmserver->scallback.on_client_disconnect(cli);

    return true;
}




// ======== Configuration - General:
/*
    @brief Function to configure the port where the server will operate.
    @note: The port value must be between 1024 and 49151 (registered ports) and 
    the configuration must be done after initialization OR after server closed.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param int port: Port number.

    @retval true: Port assign succeeded.
    @retval false: Port assign failed.
*/
bool dmserver_conf_port(dmserver_pt dmserver, int port){
    // Reference & state & values bound check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;
    if ((port < 1024) || (port > 49151)) return false;

    // Assignation to server port:
    dmserver->sconn.sport = port;
    return true;
}

/*
    @brief Function to configure the socket addres family to operate the server.
    @note: This function must be called after initialization OR after closing the server, not
    inbetween.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param sa_family_t safamily: Address family to use.

    @retval true: Socket address family assign succeeded.
    @retval false: Socket address family assign failed.
*/
bool dmserver_conf_safamily(dmserver_pt dmserver, sa_family_t safamily){
    // Reference & state & values check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;
    if ((safamily != AF_INET) && (safamily != AF_INET6)) return false;

    // Assignation to server address family:
    dmserver->sconn.sfamily = safamily;

    return true;
}

/*
    @brief Function to configure (when family selected is IPv6) if accept only IPv6 addresses or both v6 and v4.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param bool ipv6only: Boolean to enable or disable IPv6 addresses only.

    @retval true: Assignation succeeded.
    @retval false: Assignation failed.
*/
bool dmserver_conf_ipv6only(dmserver_pt dmserver, bool ipv6only){
    // Reference check:
    if (!dmserver) return false;

    // Assign boolean:
    dmserver->sconn.ss6only = ipv6only;
    return true;
}

/*
    @brief Function to establish the certificate path of the server.
    @note: This function must be called after initialization OR after server close.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param const char * certpath: Reference to the certificate path.

    @retval false: If certificate path assignation failed.
    @retval true: If certificate path assignation succeeded.
*/
bool dmserver_conf_certpath(dmserver_pt dmserver, const char * certpath){
    // References & state check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Certificate path copy:
    strncpy(dmserver->sconn.sssl_certpath, certpath, DEFAULT_SCONN_CERTPATHLEN);
    dmserver->sconn.sssl_certpath[DEFAULT_SCONN_CERTPATHLEN - 1] = '\0';

    return true;
}

/*
    @brief Function to establish the key path of the server.
    @note: This function must be called after initialization OR after server close.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param const char * keypath: Reference to the key path.

    @retval false: If key path assignation failed.
    @retval true: If key path assignation succeeded.
*/
bool dmserver_conf_keypath(dmserver_pt dmserver, const char * keypath){
    // References & state check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Certificate path copy:
    strncpy(dmserver->sconn.sssl_keypath, keypath, DEFAULT_SCONN_KEYPATHLEN);
    dmserver->sconn.sssl_keypath[DEFAULT_SCONN_CERTPATHLEN - 1] = '\0';

    return true;
}

/*
    @brief Function to enable tls(v1.3 - forced) encryption over sockets.
    @note: This function must be called after initialization OR after server close.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param bool tlsv13en: Boolean value to enable or disable tlsv13 encryption (only option of encryption for now.)

    @retval false: If enable assignation failed.
    @retval true: If enable assignation succeeded.
*/
bool dmserver_conf_tlsenable(dmserver_pt dmserver, bool tlsv13en){
    // References & state check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // SSL enable set:
    dmserver->sconn.sssl_enable = tlsv13en;

    return true;
}




// ======== Configuration - Callbacks:
/*
    @brief Function to set a callback function when a client connects to the server.
    @note: This function must be called after initialization OR after server close.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param void (*on_client_connect)(void *): Reference to callback function.

    @retval true: Callback assignation succeeded.
    @retval false: Callback assignation failed.
*/
bool dmserver_setcb_onclientconnect(dmserver_pt dmserver, void (*on_client_connect)(dmserver_cliconn_pt)){
    // References & state check:
    if (!dmserver || !on_client_connect) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Callback assignation:
    dmserver->scallback.on_client_connect = on_client_connect;
    return true;
}

/*
    @brief Function to set a callback function when a client disconnects to the server.
    @note: This function must be called after initialization OR after server close.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param void (*on_client_disconnect)(void *): Reference to callback function.

    @retval true: Callback assignation succeeded.
    @retval false: Callback assignation failed.
*/
bool dmserver_setcb_onclientdisconnect(dmserver_pt dmserver, void (*on_client_disconnect)(dmserver_cliconn_pt)){
    // References & state check:
    if (!dmserver || !on_client_disconnect) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Callback assignation:
    dmserver->scallback.on_client_disconnect = on_client_disconnect;
    return true;
}

/*
    @brief Function to set a callback function when a client timedout to the server.
    @note: This function must be called after initialization OR after server close.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param void (*on_client_timeout)(void *): Reference to callback function.

    @retval true: Callback assignation succeeded.
    @retval false: Callback assignation failed.
*/
bool dmserver_setcb_onclienttimeout(dmserver_pt dmserver, void (*on_client_timeout)(dmserver_cliconn_pt)){
    // References & state check:
    if (!dmserver || !on_client_timeout) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Callback assignation:
    dmserver->scallback.on_client_timeout = on_client_timeout;
    return true;
}

/*
    @brief Function to set a callback function when received data from a client.
    @note: This function must be called after initialization OR after server close.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param void (*on_client_rcv)(void *): Reference to callback function.

    @retval true: Callback assignation succeeded.
    @retval false: Callback assignation failed.
*/
bool dmserver_setcb_onclientrcv(dmserver_pt dmserver, void (*on_client_rcv)(dmserver_cliconn_pt)){
    // References & state check:
    if (!dmserver || !on_client_rcv) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Callback assignation:
    dmserver->scallback.on_client_rcv = on_client_rcv;
    return true;
}

/*
    @brief Function to set a callback function when sent data from a client.
    @note: This function must be called after initialization OR after server close.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param void (*on_client_snd)(void *): Reference to callback function.

    @retval true: Callback assignation succeeded.
    @retval false: Callback assignation failed.
*/
bool dmserver_setcb_onclientsnd(dmserver_pt dmserver, void (*on_client_snd)(dmserver_cliconn_pt)){
    // References & state check:
    if (!dmserver || !on_client_snd) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Callback assignation:
    dmserver->scallback.on_client_snd = on_client_snd;
    return true;
}