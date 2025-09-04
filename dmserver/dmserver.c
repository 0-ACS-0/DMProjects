/* ---- Library --------------------------------------------------- */
#include "dmserver.h"

/* ---- Hidden (internal) data structures ------------------------- */
typedef struct dmserver_subthreads_args{
    dmserver_pt dmserver;
    size_t subthindex;
}dmserver_subthargs_t;

/* ---- Static function prototypes -------------------------------- */

// Internal - Dmserver-sconn init / deinit:
static bool _dmserver_sconn_init(struct dmserver_servconn * s);
static bool _dmserver_sconn_deinit(struct dmserver_servconn * s);
static bool _dmserver_sconn_sslinit(struct dmserver_servconn * s);
static bool _dmserver_sconn_ssldeinit(struct dmserver_servconn * s);
static bool _dmserver_sconn_listen(struct dmserver_servconn * s);

// Internal - Dmserver-cconn init/reset/set/deinit:
static bool _dmserver_cconn_init(struct dmserver_cliconn * c);
static bool _dmserver_cconn_deinit(struct dmserver_cliconn * c);
static bool _dmserver_cconn_set(struct dmserver_cliconn * c, int cfd, struct sockaddr_storage * caddr, SSL * cssl);
static bool _dmserver_cconn_reset(struct dmserver_cliconn * c);
static bool _dmserver_cconn_checktimeout(struct dmserver_cliconn * c, time_t timeout_sec);
static void _dmserver_cconn_disconnect(dmserver_pt dmserver, size_t dmthindex, struct dmserver_cliconn * c);

// Internal - Dmserver-worker main/sub threads:
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
    
    // Ignore sigpipe signal:
    signal(SIGPIPE, SIG_IGN);

    // Dmserver-state initialized:
    (*dmserver)->sstate = DMSERVER_STATE_INITIALIZED;
    dmlogger_log((*dmserver)->slogger, DMLOGGER_LEVEL_INFO, "DMServer initialization completed at (%p).\n", (*dmserver));
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
    if ((*dmserver)->slogger) {dmlogger_log((*dmserver)->slogger, DMLOGGER_LEVEL_INFO, "DMServer at (%p) deinitialized.\n", (*dmserver)); dmlogger_deinit(&(*dmserver)->slogger);}

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
    if (!_dmserver_sconn_sslinit(&dmserver->sconn)) {_dmserver_sconn_deinit(&dmserver->sconn); return false;}
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_open() - server connection data initialized.");

    // Start listening on server socket:
    if (!_dmserver_sconn_listen(&dmserver->sconn)) {_dmserver_sconn_ssldeinit(&dmserver->sconn); _dmserver_sconn_deinit(&dmserver->sconn); return false;}
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_open() - server listening, with a backlog of size %d.", SOMAXCONN);

    // Server state update:
    dmserver->sstate = DMSERVER_STATE_OPENED;
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "DMServer opened.\n");
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
    _dmserver_sconn_ssldeinit(&dmserver->sconn);
    _dmserver_sconn_deinit(&dmserver->sconn);
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "dmserver_close() - server connection data deinitialized.");

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
    // Reference & cert/key paths check:
    if (!s) return false;
    if ((strlen(s->sssl_certpath) <= 0) || (strlen(s->sssl_keypath) <= 0)) return false; 

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
    // Reference check:
    if (!s) return false;

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
static bool _dmserver_cconn_set(struct dmserver_cliconn * c, int cfd, struct sockaddr_storage * caddr, SSL * cssl){
    // References & values & state checks:
    if (!c || (cfd < 0) || !caddr || !cssl) return false;
    if (c->cstate != DMSERVER_CLIENT_STANDBY) return false;

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
    SSL_shutdown(c->cssl);
    SSL_free(c->cssl);
    close(c->cfd);

    // Client state to closed:
    c->cstate = DMSERVER_CLIENT_CLOSED;

    // Client structure reset:
    _dmserver_cconn_reset(c);
    dmserver->sworker.wccount[dmthindex]--;

    // User specific data processing of disconnected client:
    if (dmserver->sworker.on_client_disconnect) dmserver->sworker.on_client_disconnect(c);
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
    if (epoll_ctl(dmserver->sworker.wmainepfd, EPOLL_CTL_ADD, dmserver->sconn.sfd, &(struct epoll_event){.events=EPOLLIN, .data.fd=dmserver->sconn.sfd}) < 0) return NULL;
    struct epoll_event ev;
    

    while (dmserver->sstate == DMSERVER_STATE_RUNNING){
        // Epoll wait for connection (or forced wakeup via wthctl):
        int nfds = epoll_wait(dmserver->sworker.wmainepfd, &ev, 1, 1000);
        if (nfds < 0  && (errno == EINTR)) {continue;}

        // Accept connection:
        int temp_cfd;
        struct sockaddr_storage temp_caddr;
        socklen_t temp_caddrlen = sizeof(temp_caddr);
        temp_cfd = accept4(dmserver->sconn.sfd, (struct sockaddr *)&temp_caddr, &temp_caddrlen, SOCK_NONBLOCK);
        if (temp_cfd < 0){continue;}

        // Create ssl session:
        SSL * temp_cssl;
        temp_cssl = SSL_new(dmserver->sconn.sssl_ctx);
        if (!temp_cssl) {close(temp_cfd); continue;}
        SSL_set_fd(temp_cssl, temp_cfd);

        if(!_dmserver_helper_sslhandshake(temp_cssl, &temp_cfd)) continue;
        
        // Distribute client to the less populated subordinate thread and the next free slot:
        size_t temp_thindex = 0;
        for (size_t i = 0; i < DEFAULT_WORKER_SUBTHREADS; i++){
            if (dmserver->sworker.wccount[i] < dmserver->sworker.wccount[temp_thindex]) temp_thindex = i;
        }
        size_t temp_cindex = 0;
        for (size_t i = 0; i < DEFAULT_WORKER_CLISPERSTH; i++){
            if (dmserver->sworker.wcclis[temp_thindex][i].cstate == DMSERVER_CLIENT_STANDBY) {temp_cindex = i; break;}
        }

        // Set the connection data into the selected client slot & add fd to the subthread epoll:
        if(!_dmserver_cconn_set(&dmserver->sworker.wcclis[temp_thindex][temp_cindex], temp_cfd, &temp_caddr, temp_cssl)) {SSL_shutdown(temp_cssl); SSL_free(temp_cssl); close(temp_cfd); continue;}
        if (epoll_ctl(dmserver->sworker.wsubepfd[temp_thindex], EPOLL_CTL_ADD, temp_cfd, &(struct epoll_event){.events=EPOLLIN, .data.ptr=&dmserver->sworker.wcclis[temp_thindex][temp_cindex]}) < 0) {_dmserver_cconn_reset(&dmserver->sworker.wcclis[temp_thindex][temp_cindex]); SSL_shutdown(temp_cssl); SSL_free(temp_cssl); close(temp_cfd); continue;}
        dmserver->sworker.wccount[temp_thindex]++;
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
        int nfds = epoll_wait(dmserver->sworker.wsubepfd[dmthindex], evs, DEFAULT_WORKER_CLISPERSTH, 1000);
        if ((nfds <= 0)  || (errno == EINTR)) {continue;}
        
        for (size_t i = 0; i < nfds; i++){
            // Obtain the pointer and check the state of the client that generated the event:
            struct dmserver_cliconn * dmclient = evs[i].data.ptr;
            if (!dmclient || (dmclient->cstate != DMSERVER_CLIENT_ESTABLISHED)) continue;

            // Check timeout & update:
            if(!_dmserver_helper_cctimeout(dmserver, dmclient, dmthindex)) continue;

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
    // Refernces check:
    if (!dmserver || !dmclient) return false;

    // Timeout check and process:
    if(!_dmserver_cconn_checktimeout(dmclient, DEFAULT_WORKER_CLITIMEOUT)){
        if (dmserver->sworker.on_client_timeout) dmserver->sworker.on_client_timeout(dmclient);
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

        int rb = SSL_read(dmclient->cssl, dmclient->crbuffer + dmclient->croff, DEFAULT_CCONN_RBUFFERLEN - dmclient->croff);
        int rb_err = SSL_get_error(dmclient->cssl, rb);
                
        if (rb > 0){
            // Data reception case:
            dmclient->crlen += rb;

            // User specific data processing of received data:
            if (dmserver->sworker.on_client_rcv) dmserver->sworker.on_client_rcv(dmclient);

            // Timeout ctl update:
            dmclient->clastt =  time(NULL);
        } else if ((rb == 0) || (rb_err == SSL_ERROR_ZERO_RETURN)){
            // Client disconnect case: 
            _dmserver_cconn_disconnect(dmserver, dmthindex, dmclient);
            pthread_mutex_unlock(&dmclient->crlock);
            return false;

        } else if ((rb_err != SSL_ERROR_WANT_READ) && (rb_err != SSL_ERROR_WANT_WRITE)){
            // Comunication error case:
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

        int wb = SSL_write(dmclient->cssl, dmclient->cwbuffer + dmclient->cwoff, dmclient->cwlen - dmclient->cwoff);

        if (wb > 0){
            // Data sended case:
            dmclient->cwoff += wb;

            // Timeout ctl update:
            dmclient->clastt =  time(NULL);

            // All data sended case:
            if (dmclient->cwoff != dmclient->cwlen) {pthread_mutex_unlock(&dmclient->cwlock); return true;}
            dmclient->cwoff = 0;
            dmclient->cwlen = 0;

            if (epoll_ctl(dmserver->sworker.wsubepfd[dmthindex], EPOLL_CTL_MOD, dmclient->cfd, &(struct epoll_event){.events=EPOLLIN, .data.ptr=dmclient}) < 0){
                // All data send error case:
                _dmserver_cconn_disconnect(dmserver, dmthindex, dmclient);
                pthread_mutex_unlock(&dmclient->cwlock); 
                return false;
            }

            // Write data user callback:
            if (dmserver->sworker.on_client_snd) dmserver->sworker.on_client_snd(dmclient);
             
        } else {
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