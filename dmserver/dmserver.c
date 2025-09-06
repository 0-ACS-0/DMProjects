/* ---- Library --------------------------------------------------- */
#include "dmserver.h"

/* ---- Hidden (internal) data structures ------------------------- */
typedef struct dmserver_subthreads_args{
    dmserver_pt dmserver;
    size_t subthindex;
}dmserver_subthargs_t;


/* ---- Static function prototypes -------------------------------- */
// Internal - Dmserver-sconn init / deinit / sslinit / ssldeinit / listen:
static bool _dmserver_sconn_init(struct dmserver_servconn * s);
static bool _dmserver_sconn_deinit(struct dmserver_servconn * s);
static bool _dmserver_sconn_sslinit(struct dmserver_servconn * s);
static bool _dmserver_sconn_ssldeinit(struct dmserver_servconn * s);
static bool _dmserver_sconn_listen(struct dmserver_servconn * s);

// Internal - Dmserver-cconn init / deinit / set / reset / checktimeout / disconnect:
static bool _dmserver_cconn_init(struct dmserver_cliconn * c);
static bool _dmserver_cconn_deinit(struct dmserver_cliconn * c);
static bool _dmserver_cconn_set(struct dmserver_cliconn * c, dmserver_cliloc_pt cloc, int cfd, struct sockaddr_storage * caddr, SSL * cssl);
static bool _dmserver_cconn_reset(struct dmserver_cliconn * c);
static bool _dmserver_cconn_checktimeout(struct dmserver_cliconn * c, time_t timeout_sec);
static void _dmserver_cconn_disconnect(dmserver_pt dmserver, size_t dmthindex, struct dmserver_cliconn * c);

// Internal - Dmserver-worker main/sub threads + helpers:
static void * _dmserver_worker_main(void * args);
static void * _dmserver_worker_sub(void * args);
static bool _dmserver_helper_sslhandshake(SSL * temp_cssl, int * temp_cfd);
static bool _dmserver_helper_cctimeout(dmserver_pt dmserver, struct dmserver_cliconn * dmclient, size_t dmthindex);
static bool _dmserver_helper_ccread(dmserver_pt dmserver, struct dmserver_cliconn * dmclient, size_t dmthindex, struct epoll_event * evs, size_t evindex);
static bool _dmserver_helper_ccwrite(dmserver_pt dmserver, struct dmserver_cliconn * dmclient, size_t dmthindex, struct epoll_event * evs, size_t evindex);


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

// ======== Broadcast / Unicast:
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
bool dmserver_broadcast(dmserver_pt dmserver, const char * bcdata){
    // References & state check:
    if (!dmserver || !bcdata) return false;
    if (dmserver->sstate != DMSERVER_STATE_RUNNING) return false;
    if (strlen(bcdata) >= DEFAULT_CCONN_WBUFFERLEN) return false;

    // Broadcast write to every connected client:
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Starting broadcast...");
    for (size_t i = 0; i < DEFAULT_WORKER_SUBTHREADS; i++){for (size_t j = 0; j < DEFAULT_WORKER_CLISPERSTH; j++){
        struct dmserver_cliconn * dmclient = &dmserver->sworker.wcclis[i][j];
        if (dmclient->cstate != DMSERVER_CLIENT_ESTABLISHED) continue;

        // Copy broadcast data to the client write buffer:
        pthread_mutex_lock(&dmclient->cwlock);

        strncpy(dmclient->cwbuffer, bcdata, DEFAULT_CCONN_WBUFFERLEN - 1);
        dmclient->cwbuffer[DEFAULT_CCONN_WBUFFERLEN - 1] = '\0';
        dmclient->cwlen = strlen(dmclient->cwbuffer);
        dmclient->cwoff = 0;

        pthread_mutex_unlock(&dmclient->cwlock);

        // Enable the output event on his epoll file descriptor:
        if (epoll_ctl(dmserver->sworker.wsubepfd[i], EPOLL_CTL_MOD, dmclient->cfd, &(struct epoll_event){.events=EPOLLIN|EPOLLOUT, .data.ptr=dmclient}) < 0) {
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
    @param dmserver_cli
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
    dmclient->cwoff = 0;

    pthread_mutex_unlock(&dmclient->cwlock);

    // Enable the output event on his epoll file descriptor:
    if (epoll_ctl(dmserver->sworker.wsubepfd[dmcliloc->th_pos], EPOLL_CTL_MOD, dmclient->cfd, &(struct epoll_event){.events=EPOLLIN|EPOLLOUT, .data.ptr=dmclient}) < 0) {
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "Unicast not queued.");
        return false;
    }
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "Unicast queued.");

    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Unicast finalized.\n");
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
bool dmserver_setcb_onclientconnect(dmserver_pt dmserver, void (*on_client_connect)(void *)){
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
bool dmserver_setcb_onclientdisconnect(dmserver_pt dmserver, void (*on_client_disconnect)(void *)){
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
bool dmserver_setcb_onclienttimeout(dmserver_pt dmserver, void (*on_client_timeout)(void *)){
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
bool dmserver_setcb_onclientrcv(dmserver_pt dmserver, void (*on_client_rcv)(void *)){
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
bool dmserver_setcb_onclientsnd(dmserver_pt dmserver, void (*on_client_snd)(void *)){
    // References & state check:
    if (!dmserver || !on_client_snd) return false;
    if ((dmserver->sstate != DMSERVER_STATE_INITIALIZED) && (dmserver->sstate != DMSERVER_STATE_CLOSED)) return false;

    // Callback assignation:
    dmserver->scallback.on_client_snd = on_client_snd;
    return true;
}







/* ---- Static function implementation ---------------------------- */
// ======== Dmserver-sconn init / sslinit / ssldeinit / deinit:
/*
    @brief Function to initialize the server connection.

    @param struct dmserver_servconn *s: Reference to dmserver sconn struct.

    @retval true: Initialization succeeded.
    @retval false: Initialization failed.
*/
static bool _dmserver_sconn_init(struct dmserver_servconn * s){
    // Reference check:
    if (!s) return false;

    // Socket file descriptor tcp, close at exec() & socket non-blocking:
    s->sfd = socket(s->sfamily, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (s->sfd < 0) return false;

    // Socket configuration:
    int sopt;
    sopt = true;
    if (setsockopt(s->sfd, SOL_SOCKET, SO_REUSEADDR, &sopt, sizeof(sopt)) < 0){_dmserver_sconn_deinit(s); return false;}
    if (setsockopt(s->sfd, SOL_SOCKET, SO_REUSEPORT, &sopt, sizeof(sopt)) < 0){_dmserver_sconn_deinit(s); return false;}
    if (s->sfamily == AF_INET6){
        sopt = s->ss6only;
        if (setsockopt(s->sfd, IPPROTO_IPV6, IPV6_V6ONLY, &sopt, sizeof(sopt)) < 0) {_dmserver_sconn_deinit(s); return false;}
    }

    // Socket address structure & bind:
    memset(&s->saddr, 0, sizeof(s->saddr));
    switch(s->sfamily){
        case AF_INET:
            s->saddr.s4.sin_addr.s_addr = INADDR_ANY;
            s->saddr.s4.sin_port = htons(s->sport);
            s->saddr.s4.sin_family = s->sfamily;
            if (bind(s->sfd, (struct sockaddr *)&s->saddr.s4, sizeof(s->saddr.s4)) < 0){_dmserver_sconn_deinit(s); return false;}
        break;

        case AF_INET6:
            s->saddr.s6.sin6_addr = in6addr_any;
            s->saddr.s6.sin6_port = htons(s->sport);
            s->saddr.s6.sin6_family = s->sfamily;
            if (bind(s->sfd, (struct sockaddr *)&s->saddr.s6, sizeof(s->saddr.s6)) < 0){_dmserver_sconn_deinit(s); return false;}
        break;

        default:
            _dmserver_sconn_deinit(s);
            return false;
    }
    return true;
}

/*
    @brief Function to deinitialize the server connection.

    @param struct dmserver_servconn *s: Reference to dmserver sconn struct.

    @retval true: Deinitialization succeeded.
    @retval false: Deinitialization failed. 
*/
static bool _dmserver_sconn_deinit(struct dmserver_servconn * s){
    // Reference check:
    if (!s) return false;

    // Close server socket:
    if (s->sfd >= 0) {close(s->sfd); s->sfd = -1;}
    return true;
}

/*
    @brief Function to initialize the server ssl service.

    @param struct dmserver_servconn *s: Reference to dmserver sconn struct.

    @retval true: Ssl initialization succeeded.
    @retval false: Ssl initialization failed. 
*/
static bool _dmserver_sconn_sslinit(struct dmserver_servconn * s){
    // Reference & en/cert/key paths check:
    if (!s) return false;
    if (!s->sssl_enable || (strlen(s->sssl_certpath) <= 0) || (strlen(s->sssl_keypath) <= 0)) return false; 

    // SSL lib initialization (only once per program!):
    static bool is_ssllib_initialized = false;
    if (!is_ssllib_initialized) {OPENSSL_init_ssl(0, NULL); is_ssllib_initialized = true;}
    
    // SSL method TLS(bc tcp):
    s->sssl_method = TLS_server_method();

    // SSL context:
    s->sssl_ctx = SSL_CTX_new(s->sssl_method);
    if (!s->sssl_ctx) return false;

    // Only TLS 1.3:
    SSL_CTX_set_min_proto_version(s->sssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(s->sssl_ctx, TLS1_3_VERSION);

    // Disable renegotiation:
    SSL_CTX_set_options(s->sssl_ctx, SSL_OP_NO_RENEGOTIATION);

    // SSL server cert. and key & validation:
    if (SSL_CTX_use_certificate_file(s->sssl_ctx, s->sssl_certpath, SSL_FILETYPE_PEM) <= 0){
        _dmserver_sconn_ssldeinit(s);
        return false;
    }

    if (SSL_CTX_use_PrivateKey_file(s->sssl_ctx, s->sssl_keypath, SSL_FILETYPE_PEM) <= 0){
        _dmserver_sconn_ssldeinit(s);
        return false;
    }

    if (!SSL_CTX_check_private_key(s->sssl_ctx)){
        _dmserver_sconn_ssldeinit(s);
        return false;
    }
    return true;
}

/*
    @brief Function to deinitialize the server ssl service.

    @param struct dmserver_servconn *s: Reference to dmserver sconn struct.

    @retval true: Ssl deinitialization succeeded.
    @retval false: Ssl deinitialization failed. 
*/
static bool _dmserver_sconn_ssldeinit(struct dmserver_servconn * s){
    // Reference & en check:
    if (!s) return false;
    if (!s->sssl_enable) return false;

    // SSL context free:
    if(s->sssl_ctx) {SSL_CTX_free(s->sssl_ctx); s->sssl_ctx = NULL;}
    return true;
}

/*
    @brief Function to put the server socket to listen connections.

    @param struct dmserver_servconn *s: Reference to dmserver sconn struct.

    @retval true: Listen succeeded.
    @retval false: Listen failed. 
*/
static bool _dmserver_sconn_listen(struct dmserver_servconn * s){
    // Reference check:
    if (!s) return false;

    // Socket listen:
    if (listen(s->sfd, SOMAXCONN) < 0) return false;
    return true;
}

// ======== Dmserver-cconn init/reset/set/deinit:
/*
    @brief Function to initialize a client connection slot.

    @param struct dmserver_cliconn *c: Reference to client slot of dmserver-worker.

    @retval true: Initialization succeeded.
    @retval false: Initialization failed.
*/
static bool _dmserver_cconn_init(struct dmserver_cliconn * c){
    // Reference & state check:
    if (!c) return false;
    if (c->cstate != DMSERVER_CLIENT_UNABLE) return false;

    // Initialize conection data:
    c->cfd = -1;
    c->caddr_family = AF_UNSPEC;

    // Initialize mutex logic:
    if(pthread_mutex_init(&c->crlock, NULL)) return false;
    if(pthread_mutex_init(&c->cwlock, NULL)) {pthread_mutex_destroy(&c->crlock); return false;}

    // Initialize state:
    c->cstate = DMSERVER_CLIENT_STANDBY;
    return true;
}

/*
    @brief Function to deinitialize a client slot.

    @param struct dmerver_cliconn *c: Reference to client slot of dmserver-worker.

    @retval true: Deinitialization succeeded.
    @retval false: Deinitialization failed.
*/
static bool _dmserver_cconn_deinit(struct dmserver_cliconn * c){
    // Reference & state check:
    if (!c) return false;
    if (c->cstate == DMSERVER_CLIENT_UNABLE) return false;
    if ((c->cstate != DMSERVER_CLIENT_CLOSED) && (c->cstate != DMSERVER_CLIENT_STANDBY)) return false;

    // Deinitialize connection data:
    c->cfd = -1;
    c->caddr_family = AF_UNSPEC;

    // Deinitialize mutex logic:
    pthread_mutex_destroy(&c->crlock);
    pthread_mutex_destroy(&c->cwlock);

    // Deinitialize state:
    c->cstate = DMSERVER_CLIENT_UNABLE;
    return true;
}

/*
    @brief Function to set all the connection data to a client slot.
    @note: Before setting all the information given, a full connection (accept + ssl) should be stablished 
    in order to copy correctly all the client socket file descripotr, address structure and ssl reference.

    @param struct dmerver_cliconn *c: Reference to client slot of dmserver-worker.
    @param int cfd: client-socket file descriptor.
    @param struct sockaddr_storage *caddr: Client connection address.
    @param SSL * cssl: Reference to client secure socket layer.

    @retval true: Set succeeded.
    @retval false: Set failed.
*/
static bool _dmserver_cconn_set(struct dmserver_cliconn * c, dmserver_cliloc_pt cloc, int cfd, struct sockaddr_storage * caddr, SSL * cssl){
    // References & values & state checks:
    if (!c || (cfd < 0) || !caddr) return false;
    if (c->cstate != DMSERVER_CLIENT_STANDBY) return false;

    // Set the client location:
    c->cloc.th_pos = cloc->th_pos;
    c->cloc.wc_pos = cloc->wc_pos;

    // Set socket file descriptor:
    c->cfd = cfd;

    // Set the address info and connection data into client (by the given IP protocl):
    c->caddr_family = caddr->ss_family;
    if (caddr->ss_family == AF_INET){
        struct sockaddr_in * addr4 = (struct sockaddr_in *)caddr;
        c->caddr.c4 = *addr4;
    } else if (caddr->ss_family == AF_INET6){
        struct sockaddr_in6 * addr6 = (struct sockaddr_in6 *)caddr;
        if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {c->caddr.c4.sin_family = AF_INET; c->caddr.c4.sin_port = addr6->sin6_port; memcpy(&c->caddr.c4.sin_addr, &addr6->sin6_addr.s6_addr[12], 4);}
        else c->caddr.c6 = *addr6;
    }

    // Set ssl session reference:
    c->cssl = cssl;

    // Set last time timeout to now:
    c->clastt = time(NULL);

    // Set established state:
    c->cstate = DMSERVER_CLIENT_ESTABLISHED;
    return true;
}

/*
    @brief Function to reset a client slot.
    @note: Before reseting the information in the client slot, the connection must be closed correctly
    outside this function, or it will lead to important leaks!

    @param struct dmerver_cliconn *c: Reference to client slot of dmserver-worker.

    @retval true: Reset succeeded.
    @retval false: Reset failed.
*/
static bool _dmserver_cconn_reset(struct dmserver_cliconn * c){
    // Reference & state check:
    if (!c) return false;
    if (c->cstate != DMSERVER_CLIENT_CLOSED) return false;

    // Reset location data:
    c->cloc.th_pos = 0;
    c->cloc.wc_pos = 0;

    // Reset connection data:
    c->cssl = NULL;
    c->cfd = -1;
    
    c->caddr_family = AF_UNSPEC;
    memset(&c->caddr, 0, sizeof(c->caddr));
    c->clastt = 0;

    // Reset read/write buffers:
    memset(c->crbuffer, 0, DEFAULT_CCONN_RBUFFERLEN);
    c->crlen = c->croff = 0;

    memset(c->cwbuffer, '\0', DEFAULT_CCONN_WBUFFERLEN);
    c->cwlen = c->cwoff = 0;

    // Reset state:
    c->cstate = DMSERVER_CLIENT_STANDBY;

    return true;
}

/*
    @brief Function to check the client timeout field.

    @param struct dmserver_cliconn * c: Reference to client to check.

    @retval true: No timeout nor fail occured.
    @retval false: Error or timeout occured.
*/
static bool _dmserver_cconn_checktimeout(struct dmserver_cliconn * c, time_t timeout_sec){
    // Reference check:
    if (!c || (timeout_sec == 0)) return false;

    // Timeout check: 
    if ((time(NULL) - c->clastt) > timeout_sec) return false;
    return true;
}

/*
    @brief Function to safely disconnect a client that is currently connected.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param size_t dmthindex: Index of the dmserver thread where client to disconnect belongs.
    @param struct dmserver_cliconn * c: Reference to client.
*/
static void _dmserver_cconn_disconnect(dmserver_pt dmserver, size_t dmthindex, struct dmserver_cliconn * c){
    // Reference & state check:
    if (!c || !dmserver) return;
    if (c->cstate != DMSERVER_CLIENT_ESTABLISHED) return;

    // Client socket file descriptor deletion from epoll:
    epoll_ctl(dmserver->sworker.wsubepfd[dmthindex], EPOLL_CTL_DEL, c->cfd, NULL);

    // Disconnection proccess:
    if (dmserver->sconn.sssl_enable){SSL_shutdown(c->cssl); SSL_free(c->cssl);}
    close(c->cfd);

    // Client state to closed:
    c->cstate = DMSERVER_CLIENT_CLOSED;

    // Client structure reset:
    _dmserver_cconn_reset(c);
    dmserver->sworker.wccount[dmthindex]--;

    // User specific data processing of disconnected client:
    if (dmserver->scallback.on_client_disconnect) dmserver->scallback.on_client_disconnect(c);
}

// ======== Dmserver-worker main/sub threads:
/*
    @brief Function that implements the main thread of dmserver.
    Accept connections and distribute clients to subordinated threads.

    @param void * args: Reference to the dmserver struct.

    @retval NULL. (always)
*/
static void * _dmserver_worker_main(void * args){
    // Reference check and cast:
    if (!args) return NULL;
    dmserver_pt dmserver = (dmserver_pt)args;

    // Prepare the main thread epoll to optimize CPU usage:
    if (epoll_ctl(dmserver->sworker.wmainepfd, EPOLL_CTL_ADD, dmserver->sconn.sfd, &(struct epoll_event){.events=EPOLLIN, .data.fd=dmserver->sconn.sfd}) < 0) 
        return NULL;
    struct epoll_event ev;
    
    while (dmserver->sstate == DMSERVER_STATE_RUNNING){
        // Epoll wait for connection (or forced wakeup via wthctl):
        int nfds = epoll_wait(dmserver->sworker.wmainepfd, &ev, 1, 2000);
        if (nfds < 0  && (errno == EINTR)) continue;

        // Accept connection:
        int temp_cfd;
        struct sockaddr_storage temp_caddr;
        socklen_t temp_caddrlen = sizeof(temp_caddr);
        temp_cfd = accept4(dmserver->sconn.sfd, (struct sockaddr *)&temp_caddr, &temp_caddrlen, SOCK_NONBLOCK);
        if (temp_cfd < 0) continue;
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "_dmserver_worker_main() - Client %d connection stage TCP ok.", temp_cfd);

        // Create ssl session:
        SSL * temp_cssl = NULL;
        if (dmserver->sconn.sssl_enable){
            temp_cssl = SSL_new(dmserver->sconn.sssl_ctx);
            if (!temp_cssl) {close(temp_cfd); continue;}
            SSL_set_fd(temp_cssl, temp_cfd);

            // Note: Semi-blocking handshake. TODO: Optimize multiple handshakes simultaneously with epoll.
            if(!_dmserver_helper_sslhandshake(temp_cssl, &temp_cfd)) continue;
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "_dmserver_worker_main() - Client %d connection stage TLS ok.", temp_cfd);
        }

        // Distribute client to the less populated subordinate thread and the next free slot:
        size_t temp_thindex = 0;
        for (size_t i = 0; i < DEFAULT_WORKER_SUBTHREADS; i++){
            if (dmserver->sworker.wccount[i] < dmserver->sworker.wccount[temp_thindex]) temp_thindex = i;
        }
        size_t temp_cindex = 0;
        for (size_t i = 0; i < DEFAULT_WORKER_CLISPERSTH; i++){
            if (dmserver->sworker.wcclis[temp_thindex][i].cstate == DMSERVER_CLIENT_STANDBY) {temp_cindex = i; break;}
        }
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "_dmserver_worker_main() - Client %d assigned to point (%lu, %lu).", temp_cfd, temp_thindex, temp_cindex);

        // Set the connection data into the selected client slot & add fd to the subthread epoll:
        if(!_dmserver_cconn_set(&dmserver->sworker.wcclis[temp_thindex][temp_cindex], &(dmserver_cliloc_t){.th_pos=temp_thindex, .wc_pos=temp_cindex}, temp_cfd, &temp_caddr, temp_cssl)) {
            if (dmserver->sconn.sssl_enable) {SSL_shutdown(temp_cssl); SSL_free(temp_cssl);}
            printf("AAAAAAA\n");
            close(temp_cfd); 
            continue;
        }
        if (epoll_ctl(dmserver->sworker.wsubepfd[temp_thindex], EPOLL_CTL_ADD, temp_cfd, &(struct epoll_event){.events=EPOLLIN, .data.ptr=&dmserver->sworker.wcclis[temp_thindex][temp_cindex]}) < 0) {
            _dmserver_cconn_reset(&dmserver->sworker.wcclis[temp_thindex][temp_cindex]);
            if (dmserver->sconn.sssl_enable) {SSL_shutdown(temp_cssl); SSL_free(temp_cssl);}
            close(temp_cfd);
            printf("BBBBBBBB\n");
            continue;
        }
        dmserver->sworker.wccount[temp_thindex]++;

        char cip_str[INET6_ADDRSTRLEN];
        const void * addr = (dmserver->sworker.wcclis[temp_thindex][temp_cindex].caddr_family == AF_INET) ? (void*)&dmserver->sworker.wcclis[temp_thindex][temp_cindex].caddr.c4.sin_addr : (void*)&dmserver->sworker.wcclis[temp_thindex][temp_cindex].caddr.c6.sin6_addr;
        inet_ntop(dmserver->sworker.wcclis[temp_thindex][temp_cindex].caddr_family, addr, cip_str, sizeof(cip_str));
        int cport_num = (dmserver->sworker.wcclis[temp_thindex][temp_cindex].caddr_family == AF_INET) ? ntohs(dmserver->sworker.wcclis[temp_thindex][temp_cindex].caddr.c4.sin_port) : ntohs(dmserver->sworker.wcclis[temp_thindex][temp_cindex].caddr.c6.sin6_port);
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Client %d with address %s:%d connected to server.\n", dmserver->sworker.wcclis[temp_thindex][temp_cindex].cfd, cip_str, cport_num);
    }

    // Delete the server socket file descriptor from main thread epoll:
    epoll_ctl(dmserver->sworker.wmainepfd, EPOLL_CTL_DEL, dmserver->sconn.sfd, NULL);
    return NULL;
}

/*
    @brief Function that implements the subordinated threads of dmserver.
    Manage the clients data reception and transmission.

    @param void * args: Reference to the dmserver struct.

    @retval NULL. (always)
*/
static void * _dmserver_worker_sub(void * args){
    // Reference check and cast:
    if (!args) return NULL;
    dmserver_subthargs_t * dmargs = (dmserver_subthargs_t *)args;
    dmserver_pt dmserver = dmargs->dmserver;
    size_t dmthindex = dmargs->subthindex;
    free(args);

    // Prepare the subordinate thread epoll to optimize CPU usage:
    struct epoll_event evs[DEFAULT_WORKER_CLISPERSTH];

    while (dmserver->sstate == DMSERVER_STATE_RUNNING){
        // Epoll wait for events (or forced wakeup via wthctl):
        int nfds = epoll_wait(dmserver->sworker.wsubepfd[dmthindex], evs, DEFAULT_WORKER_CLISPERSTH, 6000);
        if (nfds == 0) {for (size_t i = 0; i < DEFAULT_WORKER_CLISPERSTH; i++){_dmserver_helper_cctimeout(dmserver, &dmserver->sworker.wcclis[dmthindex][i], dmthindex); continue;}}
        if ((nfds < 0)  || (errno == EINTR)) {continue;}
        

        for (size_t i = 0; i < nfds; i++){
            // Obtain the pointer and check the state of the client that generated the event:
            struct dmserver_cliconn * dmclient = evs[i].data.ptr;
            if (!dmclient || (dmclient->cstate != DMSERVER_CLIENT_ESTABLISHED)) continue;

            // Handle read:
            if(!_dmserver_helper_ccread(dmserver, dmclient, dmthindex, evs, i)) continue;

            // Handle write:
            if(!_dmserver_helper_ccwrite(dmserver, dmclient, dmthindex, evs, i)) continue;
        }
    }

    // Completly closes the clients connections at thread exit:
    for (size_t i = 0; i < DEFAULT_WORKER_CLISPERSTH; i++){
        _dmserver_cconn_disconnect(dmserver, dmthindex, &dmserver->sworker.wcclis[dmthindex][i]);
    }

    return NULL;
}

/*
    @brief Helper function that implements the ssl handshake (in pseudo blocking mode - 20 attempts).

    @param SSL * temp_cssl: Reference to temporal client ssl object.
    @param int * temp_cfd: Reference to temporal client socket file descriptor.

    @retval false: SSL Handshake failed.
    @retval true: SSL Handshake succeeded.
*/
static bool _dmserver_helper_sslhandshake(SSL * temp_cssl, int * temp_cfd){
    // References check:
    if (!temp_cssl || !temp_cfd) return false;

    // SSL Handshake process:
    int ssl_code;
    int attempts = 0;
    int max_attempts = 20;
    bool ssl_fatalerr = false;

    do {
        ssl_code = SSL_accept(temp_cssl);
        int err = SSL_get_error(temp_cssl, ssl_code);

        switch(err){
            case SSL_ERROR_NONE:
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                // Handshake in progress:
                break;

            case SSL_ERROR_ZERO_RETURN:
            case SSL_ERROR_SSL:
            case SSL_ERROR_SYSCALL:
            default:
                // Fatal/Unknown error detected:
                ssl_fatalerr = true;
                SSL_shutdown(temp_cssl); 
                SSL_free(temp_cssl); 
                close(*temp_cfd); 
                break;
        }

        // If fatal error ocurred exits the loop:
        if (ssl_fatalerr) break;

        // Wait 50 ms to avoid CPU usage with slow clients: 
        usleep(50 * 1000);  
        
    } while ((ssl_code <= 0) && (attempts++ < max_attempts));
    if (ssl_code <= 0) return false;
    
    return true;
}

/*
    @brief Helper function that implements a client timeout process.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param struct dmserver_cliconn * dmclient: Reference to the client to check timeout.
    @param size_t dmthindex: Caller thread index.

    @retval false: Client timedout and disconnected.
    @retval true: Client not timedout.
*/
static bool _dmserver_helper_cctimeout(dmserver_pt dmserver, struct dmserver_cliconn * dmclient, size_t dmthindex){
    // Refernces & state check:
    if (!dmserver || !dmclient) return false;
    if (dmclient->cstate != DMSERVER_CLIENT_ESTABLISHED) return false;

    // Timeout check and process:
    if(!_dmserver_cconn_checktimeout(dmclient, DEFAULT_WORKER_CLITIMEOUT)){
        if (dmserver->scallback.on_client_timeout) dmserver->scallback.on_client_timeout(dmclient);
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Client %d timedout, connection closed.\n", dmclient->cfd);
        _dmserver_cconn_disconnect(dmserver, dmthindex, dmclient);
        return false;
    }

    return true;
}

/*
    @brief Helper function that implements the read process.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param struct dmserver_cliconn * dmclient: Reference to the client to check timeout.
    @param size_t dmthindex: Caller thread index.
    @param struct epoll_events * evs: Reference to the array of events generated.
    @param size_t evindex: Event to process.

    @retval false: If read process lead to client disconnection.
    @retval true: If read process finished correctly.
*/
static bool _dmserver_helper_ccread(dmserver_pt dmserver, struct dmserver_cliconn * dmclient, size_t dmthindex, struct epoll_event * evs, size_t evindex){
    // References check:
    if (!dmserver || !dmclient || !evs) return false;

    // Read process:
    if (evs[evindex].events & EPOLLIN){
        // Read lock of client:
        pthread_mutex_lock(&dmclient->crlock);

        // Read bytes from clients (encrypted/decrypted optional):
        int rb = 0;
        int rb_err = 0;
        if (dmserver->sconn.sssl_enable){
            rb = SSL_read(dmclient->cssl, dmclient->crbuffer + dmclient->croff, DEFAULT_CCONN_RBUFFERLEN - dmclient->croff);
            rb_err = SSL_get_error(dmclient->cssl, rb);
        } else {
            rb = read(dmclient->cfd, dmclient->crbuffer, DEFAULT_CCONN_RBUFFERLEN);
            rb_err = errno;
        }

                
        if (rb > 0){
            // Data reception case:
            dmclient->crlen += rb;
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "Read of %d bytes from client %d.\n", rb, dmclient->cfd);

            // Timeout ctl update:
            dmclient->clastt =  time(NULL);
            
            // User specific data processing of received data and read buffer reset afterwards:
            if (dmserver->scallback.on_client_rcv) dmserver->scallback.on_client_rcv(dmclient);
            memset(dmclient->crbuffer, 0, DEFAULT_CCONN_RBUFFERLEN);

            
        } else if ((rb == 0) || ((rb_err == SSL_ERROR_ZERO_RETURN) && dmserver->sconn.sssl_enable)){
            // Client disconnect case:
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Client %d disconnected.\n", dmclient->cfd); 
            _dmserver_cconn_disconnect(dmserver, dmthindex, dmclient);
            pthread_mutex_unlock(&dmclient->crlock);
            return false;

        } else if ((((rb_err != SSL_ERROR_WANT_READ) && (rb_err != SSL_ERROR_WANT_WRITE)) && dmserver->sconn.sssl_enable) || (((rb_err != EAGAIN) && (rb_err != EWOULDBLOCK) && (rb_err != EINTR)) && !dmserver->sconn.sssl_enable)){
            // Comunication error case:
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Client %d com. failed, forced disconnected.\n", dmclient->cfd); 
            _dmserver_cconn_disconnect(dmserver, dmthindex, dmclient);
            pthread_mutex_unlock(&dmclient->crlock);
            return false;
        }

        // Read unlock of client:
        pthread_mutex_unlock(&dmclient->crlock);
    }

    return true;
}

/*
    @brief Helper function that implements the write process.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param struct dmserver_cliconn * dmclient: Reference to the client to check timeout.
    @param size_t dmthindex: Caller thread index.
    @param struct epoll_events * evs: Reference to the array of events generated.
    @param size_t evindex: Event to process.

    @retval false: If write process lead to client disconnection.
    @retval true: If write process finished correctly.
*/
static bool _dmserver_helper_ccwrite(dmserver_pt dmserver, struct dmserver_cliconn * dmclient, size_t dmthindex, struct epoll_event * evs, size_t evindex){
    // References check:
    if (!dmserver || !dmclient || !evs) return false;
    
    // Write process:
    if ((evs[evindex].events & EPOLLOUT) && (dmclient->cwlen > 0)){
        // Write lock of client:
        pthread_mutex_lock(&dmclient->cwlock);

        // Write bytes from clients (encrypted/decrypted optional):
        int wb = 0;
        int wb_err = 0;
        if (dmserver->sconn.sssl_enable){
            wb = SSL_write(dmclient->cssl, dmclient->cwbuffer + dmclient->cwoff, dmclient->cwlen - dmclient->cwoff);
            wb_err = SSL_get_error(dmclient->cssl, wb);
        } else {
            wb = write(dmclient->cfd, dmclient->cwbuffer + dmclient->cwoff, dmclient->cwlen - dmclient->cwoff);
            wb_err = errno;
        }

        if (wb > 0){
            // Data sent case:
            dmclient->cwoff += wb;;

            // All data sent case:
            if (dmclient->cwoff != dmclient->cwlen) {pthread_mutex_unlock(&dmclient->cwlock); return true;}
            memset(dmclient->cwbuffer, 0, DEFAULT_CCONN_WBUFFERLEN);
            dmclient->cwoff = 0;
            dmclient->cwlen = 0;

            if (epoll_ctl(dmserver->sworker.wsubepfd[dmthindex], EPOLL_CTL_MOD, dmclient->cfd, &(struct epoll_event){.events=EPOLLIN, .data.ptr=dmclient}) < 0){
                // All data send error case:
                _dmserver_cconn_disconnect(dmserver, dmthindex, dmclient);
                pthread_mutex_unlock(&dmclient->cwlock); 
                return false;
            }

            // Write data user callback:
            if (dmserver->scallback.on_client_snd) dmserver->scallback.on_client_snd(dmclient);
             
        } else if ((((wb_err != SSL_ERROR_WANT_READ) && (wb_err != SSL_ERROR_WANT_WRITE)) && dmserver->sconn.sssl_enable) || (((wb_err != EAGAIN) && (wb_err != EWOULDBLOCK) && (wb_err != EINTR)) && !dmserver->sconn.sssl_enable)) {
            // Comunication error case:
            _dmserver_cconn_disconnect(dmserver, dmthindex, dmclient);
            pthread_mutex_unlock(&dmclient->cwlock); 
            return false;
        }

        // Write unlock of clients:
        pthread_mutex_unlock(&dmclient->cwlock);
    }

    return true;
}