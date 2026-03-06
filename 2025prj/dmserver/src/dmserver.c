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
    if (!(*dmserver)->slogger) {
        dmserver_deinit(dmserver);
        return;
    }
    
    if (!dmlogger_run((*dmserver)->slogger)) {
        dmserver_deinit(dmserver);
        return;
    }

    // Dmserver-sconn initialization to defaults:
    __dmserver_sconn_set_defaults(&(*dmserver)->sconn);
    
    // Dmserver-cconn (worker) initialization to defaults:
    __dmserver_worker_set_defaults(&(*dmserver)->sworker);
    __dmserver_worker_alloc(&(*dmserver)->sworker);
    
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
    __dmserver_worker_dealloc(&(*dmserver)->sworker);

    // Dmserver-logger deinitialization (internally flush and dealloc):
    if ((*dmserver)->slogger) {
        dmlogger_log((*dmserver)->slogger, DMLOGGER_LEVEL_INFO, "-------- DMServer at (%p) deinitialized.\n", (*dmserver));
        dmlogger_deinit(&(*dmserver)->slogger);
    }

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
        if (!_dmserver_sconn_sslinit(&dmserver->sconn)) {
            _dmserver_sconn_deinit(&dmserver->sconn);
            return false;
        }
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_open() - server ssl data initialized.");
    }

    // Start listening on server socket:
    if (!_dmserver_sconn_listen(&dmserver->sconn)) {
        _dmserver_sconn_ssldeinit(&dmserver->sconn);
        _dmserver_sconn_deinit(&dmserver->sconn);
        return false;
    }
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_open() - server listening, with a backlog of size %d.", SOMAXCONN);

    // Server state update:
    dmserver->sstate = DMSERVER_STATE_OPENED;

    char sip_str[INET6_ADDRSTRLEN];
    const void * addr = (dmserver->sconn.ssafamily == AF_INET) ? (void*)&dmserver->sconn.saddr.s4.sin_addr : (void*)&dmserver->sconn.saddr.s6.sin6_addr;
    inet_ntop(dmserver->sconn.ssafamily, addr, sip_str, sizeof(sip_str));
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
    for (size_t i = 0; i < dmserver->sworker.wth_subthreads; i++){
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

    for (size_t i = 0; i < dmserver->sworker.wth_subthreads; i++){
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

    // Broadcast write to every connected client:
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Starting broadcast...");
    for (size_t i = 0; i < dmserver->sworker.wth_subthreads; i++){for (size_t j = 0; j < dmserver->sworker.wth_clispersth; j++){
        // Check client broadcast condition:
        dmserver_cliconn_pt dmclient = &dmserver->sworker.wcclis[i][j];
        if (dmclient->cstate != DMSERVER_CLIENT_ESTABLISHED) continue;
        if (bexclude && (dmclient->cloc.th_pos == bexclude->th_pos) && (dmclient->cloc.wc_pos == bexclude->wc_pos)) continue;

        // Copy broadcast data to the client write buffer:
        pthread_mutex_lock(&dmclient->cwlock);

        strncpy(dmclient->cwbuffer, bcdata, dmclient->cwbuffer_size - 1);
        dmclient->cwbuffer[dmclient->cwbuffer_size - 1] = '\0';
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
    // References, state & bounds check:
    if (!dmserver || !dmcliloc || !ucdata) return false;
    if (dmserver->sstate != DMSERVER_STATE_RUNNING) return false;
    if ((dmcliloc->th_pos >= dmserver->sworker.wth_subthreads) || (dmcliloc->wc_pos >= dmserver->sworker.wth_clispersth)) return false;

    // Client established check:
    dmserver_cliconn_pt dmclient = &dmserver->sworker.wcclis[dmcliloc->th_pos][dmcliloc->wc_pos];
    if (dmclient->cstate != DMSERVER_CLIENT_ESTABLISHED) return false;

    // Copy unicast data to the client write buffer:
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Starting unicast to client %d...", dmclient->cfd);
    pthread_mutex_lock(&dmclient->cwlock);

    strncpy(dmclient->cwbuffer, ucdata, dmclient->cwbuffer_size - 1);
    dmclient->cwbuffer[dmclient->cwbuffer_size - 1] = '\0';
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
    if (dmserver->sconn.sssl_enable){
        SSL_shutdown(cli->cssl);
        SSL_free(cli->cssl);
    }
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
    @brief Function to configure the server connection data.
    @note: This function must be called after initialization OR after closing the server.

    @param dmserver_pt dmserver: Reference to server struct.
    @param dmserver_servconn_conf_pt: Reference to server connection configuration struct.

    @retval true: Configuration succeeded.
    @retval false: Configuration failed.
*/
bool dmserver_conf_sconn(dmserver_pt dmserver, dmserver_servconn_conf_pt sconn_conf){
    // Reference & state check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;


    // If there is no configuration structure, set to defaults:
    if (!sconn_conf) {
        __dmserver_sconn_set_defaults(&dmserver->sconn);
        return true;
    }

    // Server connection port configuration:
    if (sconn_conf->sport && (sconn_conf->sport >= 1024) && (sconn_conf->sport <= 65535)) __dmserver_sconn_set_port(&dmserver->sconn, sconn_conf->sport);

    // Server connection socket address configuration:
    if (sconn_conf->ssa_family && ((sconn_conf->ssa_family == AF_INET) || (sconn_conf->ssa_family == AF_INET6))) __dmserver_sconn_set_safamily(&dmserver->sconn, sconn_conf->ssa_family);

    // Server connection ipv6 only flag configuration:
    __dmserver_sconn_set_ipv6only(&dmserver->sconn, sconn_conf->sipv6_only);
    
    // Server secure socket layer encryption flag:
    __dmserver_sconn_set_tls(&dmserver->sconn, sconn_conf->stls_enable);

    // Server ssl certification and key path:
    if (sconn_conf->scert_path && (strlen(sconn_conf->scert_path) < DEFAULT_SCONN_CERTPATHLEN)) __dmserver_sconn_set_certpath(&dmserver->sconn, sconn_conf->scert_path);
    if (sconn_conf->skey_path && (strlen(sconn_conf->skey_path) < DEFAULT_SCONN_KEYPATHLEN)) __dmserver_sconn_set_keypath(&dmserver->sconn, sconn_conf->skey_path);

    return true;
}

/*
    @brief Function to configure workers parameters before launching.
    @note: This function must be called after initialization OR after closing the server.

    @param dmserver_pt dmserver: Reference to server struct.
    @param dmserver_servconn_conf_pt worker_conf: Reference to worker connection configuration struct.

    @retval true: Configuration succeeded.
    @retval false: Configuration failed.
*/
bool dmserver_conf_worker(dmserver_pt dmserver, dmserver_worker_conf_pt worker_conf){
    // Reference & state check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Deallocate previous worker configuraiton and memory:
    if (!__dmserver_worker_dealloc(&dmserver->sworker)) return false;

    // If there is no configuration given, set to defaults and exit:
    if (!worker_conf) {
        __dmserver_worker_set_defaults(&dmserver->sworker);
        __dmserver_worker_alloc(&dmserver->sworker);
        return true;
    }

    // Configure subordinate threads number, clients per subordinate thread and timeout per client:
    if (worker_conf->wth_subthreads) __dmserver_worker_set_subthreads(&dmserver->sworker, worker_conf->wth_subthreads);
    if (worker_conf->wth_clispersth) __dmserver_worker_set_clispersth(&dmserver->sworker, worker_conf->wth_clispersth);
    if (worker_conf->wth_clistimeout) __dmserver_worker_set_clistimeout(&dmserver->sworker, worker_conf->wth_clistimeout);

    if (!__dmserver_worker_alloc(&dmserver->sworker)) return false;
    return true;
}

/*
    @brief Function to configure the clients buffers length.
    @note: This function must be called after initialization OR after closing the server.

    @param dmserver_pt dmserver: Reference to server struct.
    @param dmserver_cliconn_conf_pt cconn_conf: Reference to client connection (buffers only) configuration struct.

    @retval true: Configuration succeeded.
    @retval false: Configuration failed.
*/
bool dmserver_conf_cconn(dmserver_pt dmserver, dmserver_cliconn_conf_pt cconn_conf){
    // Reference & state check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Iterate over all the clients slots on the server:
    for (size_t i = 0; i < dmserver->sworker.wth_subthreads; i++){for (size_t j = 0; j < dmserver->sworker.wth_clispersth; j++){
        // Client:
        dmserver_cliconn_pt dmclient = &dmserver->sworker.wcclis[i][j];

        // Deallocate previous read/write buffers:
        if (!__dmserver_cconn_buf_dealloc(dmclient)) continue;

        // In case that the configuration structure is null, set the default buffers length:
        if (!cconn_conf) __dmserver_cconn_set_defaults(dmclient);

        // Read/Write buffers length set:
        if (cconn_conf->cread_buffer_size) __dmserver_cconn_set_creadbuffer(dmclient, cconn_conf->cread_buffer_size);
        if (cconn_conf->cwrite_buffer_size) __dmserver_cconn_set_cwritebuffer(dmclient, cconn_conf->cwrite_buffer_size);

        // Allocate new read/write buffers:
        if (!__dmserver_cconn_buf_alloc(dmclient)) continue;
    }}
    return true;
}


// ======== Configuration - Callbacks:
/*
    @brief Function to set the callbacks available to the server, to apply external functionallity.
    @note: This function must be called after initialization OR after closing the server.

    @param dmserver_pt dmserver: Reference to server struct.
    @param dmserver_callback_conf_pt callback_conf: Reference to callbacks configuration structure.

    @retval true: Configuration succeeded.
    @retval false: Configuration failed. 
*/
bool dmserver_set_cb(dmserver_pt dmserver, dmserver_callback_conf_pt callback_conf){
    // References & state check:
    if (!dmserver) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;
    
    // In case no configuration structure is given, exist as configured:
    if (!callback_conf) return true;

    // Callbacks set:
    if (callback_conf->on_client_connect) __dmserver_setcb_onclientconnect(&dmserver->scallback, callback_conf->on_client_connect);
    if (callback_conf->on_client_disconnect) __dmserver_setcb_onclientdisconnect(&dmserver->scallback, callback_conf->on_client_disconnect);
    if (callback_conf->on_client_timeout) __dmserver_setcb_onclienttimeout(&dmserver->scallback, callback_conf->on_client_timeout);
    if (callback_conf->on_client_rcv) __dmserver_setcb_onclientrcv(&dmserver->scallback, callback_conf->on_client_rcv);
    if (callback_conf->on_client_snd) __dmserver_setcb_onclientsnd(&dmserver->scallback, callback_conf->on_client_snd);


    return true;
}
