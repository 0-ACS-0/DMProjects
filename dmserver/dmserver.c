/* ---- Library --------------------------------------------------- */
#include "dmserver.h"


/* ---- Static function prototypes -------------------------------- */

// Internal - Dmserver-sconn init / deinit:
static bool _dmserver_sconn_init(struct dmserver_servconn * s);
static bool _dmserver_sconn_deinit(struct dmserver_servconn * s);
static bool _dmserver_sconn_sslinit(struct dmserver_servconn * s);
static bool _dmserver_sconn_ssldeinit(struct dmserver_servconn * s);
static bool _dmserver_sconn_slisten(struct dmserver_servconn * s);

// Internal - Dmserver-cconn init/reset/set/deinit:
static bool _dmserver_cconn_init(struct dmserver_cliconn * c);
static bool _dmserver_cconn_set(struct dmserver_cliconn * c, int cfd, struct sockaddr * caddr, socklen_t caddrlen, SSL * cssl);
static bool _dmserver_cconn_reset(struct dmserver_cliconn * c);
static bool _dmserver_cconn_deinit(struct dmserver_cliconn * c);


// Internal - Dmserver-worker main/sub threads:
static void * _dmserver_worker_main(void * args);
static void * _dmserver_worker_sub(void * args);

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

    // Dmserver-cconn initialization to defaults:
    for (size_t i = 0; i < DEFAULT_WORKER_SUBTHREADS; i++){for (size_t j = 0; j < DEFAULT_WORKER_CLISPERSTH; j++){
        if (!_dmserver_cconn_init(&(*dmserver)->sworker.wcclis[i][j])) {dmserver_deinit(dmserver); return;}
    }}
    (*dmserver)->sworker.wctimeout = DEFAULT_WORKER_CLITIMEOUT;

    // Dmserver-state initialized:
    (*dmserver)->sstate = DMSERVER_STATE_INITIALIZED;
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
    for (size_t i = 0; i < DEFAULT_WORKER_SUBTHREADS; i++){for (size_t j = 0; j < DEFAULT_WORKER_CLISPERSTH; j++){
        _dmserver_cconn_deinit(&(*dmserver)->sworker.wcclis[i][j]);
    }}

    // Dmserver-logger deinitialization (internally flush and dealloc):
    if ((*dmserver)->slogger) dmlogger_deinit(&(*dmserver)->slogger);

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

    // Start listening on server socket:
    if (!_dmserver_sconn_slisten(&dmserver->sconn)) {_dmserver_sconn_ssldeinit(&dmserver->sconn); _dmserver_sconn_deinit(&dmserver->sconn); return false;}

    // Server state update:
    dmserver->sstate = DMSERVER_STATE_OPENED;
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

    // Server state update:
    dmserver->sstate = DMSERVER_STATE_CLOSED;

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
    sopt = s->ss6only;
    if (setsockopt(s->sfd, IPPROTO_IPV6, IPV6_V6ONLY, &sopt, sizeof(sopt)) < 0) {_dmserver_sconn_deinit(s); return false;}

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
static bool _dmserver_sconn_slisten(struct dmserver_servconn * s){
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
    if (c->cstate == DMSERVER_CLIENT_UNABLE) return false;

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
    @brief Function to set data to a client slot.

    @param struct smerver_cliconn *c: Reference to client slot of dmserver-worker.
    @param int cfd: client-socket file descriptor.
    @param struct sockaddr *caddr: Client connection address.
    @param socklen_t caddrlen: Length of the client socket address structure.
    @param SSL * cssl: Reference to client secure socket layer.

    @retval true: Set succeeded.
    @retval false: Set failed.
*/
static bool _dmserver_cconn_set(struct dmserver_cliconn * c, int cfd, struct sockaddr * caddr, socklen_t caddrlen, SSL * cssl){
    // References & values & state checks:
    if (!c || (cfd < 0) || !caddr || (caddrlen == 0) || !cssl) return false;
    if (c->cstate != DMSERVER_CLIENT_STANDBY) return false;

    // Set connection data:
    c->cfd = cfd;
    c->caddr_family = caddr->sa_family;
    c->caddr_len = caddrlen;
    if (c->caddr_family == AF_INET) {if (caddrlen != sizeof(struct sockaddr_in)) return false; memcpy(&c->caddr.c4, caddr, caddrlen);}
    else if (c->caddr_family == AF_INET6) {if (caddrlen != sizeof(struct sockaddr_in6)) return false; memcpy(&c->caddr.c6, caddr, caddrlen);}
    else return false;
    c->cssl = cssl;

    // Set last time timeout to now:
    c->clastt = time(NULL);

    // Set established state:
    c->cstate = DMSERVER_CLIENT_ESTABLISHED;
    return true;
}

/*
    @brief Function to reset a client slot.

    @param struct smerver_cliconn *c: Reference to client slot of dmserver-worker.

    @retval true: Reset succeeded.
    @retval false: Reset failed.
*/
static bool _dmserver_cconn_reset(struct dmserver_cliconn * c){
    // Reference & state check:
    if (!c) return false;
    if (c->cstate != DMSERVER_CLIENT_CLOSED) return false;

    // Reset connection data:
    c->cfd = -1;
    c->caddr_family = AF_UNSPEC;
    c->caddr_len = 0;
    memset(&c->caddr, 0, sizeof(c->caddr));
    c->cssl = NULL;
    c->clastt = 0;
    
    // Reset read/write buffers:
    pthread_mutex_lock(&c->crlock);
    memset(c->crbuffer, '\0', DEFAULT_CCONN_RBUFFERLEN);
    c->crlen = c->croff = 0;
    pthread_mutex_unlock(&c->crlock);

    pthread_mutex_lock(&c->cwlock);
    memset(c->cwbuffer, '\0', DEFAULT_CCONN_WBUFFERLEN);
    c->cwlen = c->cwoff = 0;
    pthread_mutex_unlock(&c->cwlock);

    // Reset state:
    c->cstate = DMSERVER_CLIENT_STANDBY;

    return true;
}

/*
    @brief Function to deinitialize a client slot.

    @param struct smerver_cliconn *c: Reference to client slot of dmserver-worker.

    @retval true: Deinitialization succeeded.
    @retval false: Deinitialization failed.
*/
static bool _dmserver_cconn_deinit(struct dmserver_cliconn * c){
    // Reference & state check:
    if (!c) return false;
    if ((c->cstate == DMSERVER_CLIENT_UNABLE) && (c->cstate != DMSERVER_CLIENT_CLOSED) && (c->cstate != DMSERVER_CLIENT_STANDBY)) return false;

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


// ======== Dmserver-worker main/sub threads:
/*

*/
static void * _dmserver_worker_main(void * args){
    return NULL;
}

/*

*/
static void * _dmserver_worker_sub(void * args){
    return NULL;
}