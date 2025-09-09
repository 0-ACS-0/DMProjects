/*

*/

/* ---- Library --------------------------------------------------- */
#include "../inc/dmserver_servconn.h"

/* ---- INTERNAL - Functions implementation ----------------------- */
/*
    @brief Function to initialize the server connection.

    @param struct dmserver_servconn *s: Reference to dmserver sconn struct.

    @retval true: Initialization succeeded.
    @retval false: Initialization failed.
*/
bool _dmserver_sconn_init(struct dmserver_servconn * s){
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
bool _dmserver_sconn_deinit(struct dmserver_servconn * s){
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
bool _dmserver_sconn_sslinit(struct dmserver_servconn * s){
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
bool _dmserver_sconn_ssldeinit(struct dmserver_servconn * s){
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
bool _dmserver_sconn_listen(struct dmserver_servconn * s){
    // Reference check:
    if (!s) return false;

    // Socket listen:
    if (listen(s->sfd, SOMAXCONN) < 0) return false;
    return true;
}