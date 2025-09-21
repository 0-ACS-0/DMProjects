/*

*/

/* ---- Library --------------------------------------------------- */
#include "../inc/_dmserver_servconn.h"

/* ---- INTERNAL - Functions implementation ----------------------- */
// ======== General use:
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
    s->sfd = socket(s->ssafamily, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (s->sfd < 0) return false;

    // Socket configuration:
    int sopt;
    sopt = true;
    if (setsockopt(s->sfd, SOL_SOCKET, SO_REUSEADDR, &sopt, sizeof(sopt)) < 0){_dmserver_sconn_deinit(s); return false;}
    if (setsockopt(s->sfd, SOL_SOCKET, SO_REUSEPORT, &sopt, sizeof(sopt)) < 0){_dmserver_sconn_deinit(s); return false;}
    if (s->ssafamily == AF_INET6){
        sopt = s->ss6only;
        if (setsockopt(s->sfd, IPPROTO_IPV6, IPV6_V6ONLY, &sopt, sizeof(sopt)) < 0) {_dmserver_sconn_deinit(s); return false;}
    }

    // Socket address structure & bind:
    memset(&s->saddr, 0, sizeof(s->saddr));
    switch(s->ssafamily){
        case AF_INET:
            s->saddr.s4.sin_addr.s_addr = INADDR_ANY;
            s->saddr.s4.sin_port = htons(s->sport);
            s->saddr.s4.sin_family = s->ssafamily;
            if (bind(s->sfd, (struct sockaddr *)&s->saddr.s4, sizeof(s->saddr.s4)) < 0){_dmserver_sconn_deinit(s); return false;}
        break;

        case AF_INET6:
            s->saddr.s6.sin6_addr = in6addr_any;
            s->saddr.s6.sin6_port = htons(s->sport);
            s->saddr.s6.sin6_family = s->ssafamily;
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








// ======== Configuration:
/*
    @brief Function to configure the server with defaults values.

    @param dmserver_servconn_pt s: Reference to server conn. structure.
*/
void __dmserver_sconn_set_defaults(dmserver_servconn_pt s){
    // Sockets defaults:
    s->sfd = -1;
    s->sport = DEFAULT_SCONN_SPORT;
    s->ssafamily = DEFAULT_SCONN_SFAMILY;

    // SSL Defaults:
    s->sssl_enable = DEFAULT_SCONN_SSLENABLE;
    strncpy(s->sssl_certpath, DEFAULT_SCONN_CERTPATHVAL, DEFAULT_SCONN_CERTPATHLEN);
    s->sssl_certpath[DEFAULT_SCONN_CERTPATHLEN - 1] = '\0';
    strncpy(s->sssl_keypath, DEFAULT_SCONN_KEYPATHVAL, DEFAULT_SCONN_KEYPATHLEN);
    s->sssl_keypath[DEFAULT_SCONN_KEYPATHLEN - 1] = '\0';
}

/*
    @brief Function to configure the server connection port.

    @param dmserver_servconn_pt s: Reference to server conn. structure.
    @param int s_port: Port number.
*/
void __dmserver_sconn_set_port(dmserver_servconn_pt s, int sport){
    s->sport = sport;
}

/*
    @brief Function to configure the server socket address family.
    
    @param dmserver_servconn_pt s: Reference to server conn. structure.
    @param sa_family_t sa_family: Socket address family.
*/
void __dmserver_sconn_set_safamily(dmserver_servconn_pt s, sa_family_t sa_family){
    s->ssafamily = sa_family;
}

/*
    @brief Function to configure the server address ipv6 family only option.
       
    @param dmserver_servconn_pt s: Reference to server conn. structure.
    @param bool ipv6_only: IPv6 only flag.
*/
void __dmserver_sconn_set_ipv6only(dmserver_servconn_pt s, bool sipv6_only){
    s->ss6only = sipv6_only;
}

/*
    @brief Function to configure the enable flag for tls encryption.

    @param dmserver_servconn_pt s: Reference to server conn. structure.
    @param bool tls_enable: Falg to enable tls encryption.
*/
void __dmserver_sconn_set_tls(dmserver_servconn_pt s, bool stls_enable){
    s->sssl_enable = stls_enable;
}

/*
    @brief Function to configure the route to the certificate of the server.

    @param dmserver_servconn_pt s: Reference to server conn. structure.
    @param const char * cert_path: Certificate path char string reference.
*/
void __dmserver_sconn_set_certpath(dmserver_servconn_pt s, const char * scert_path){
    strncpy(s->sssl_certpath, scert_path, DEFAULT_SCONN_CERTPATHLEN);
    s->sssl_certpath[DEFAULT_SCONN_CERTPATHLEN - 1] = '\0';
}

/*
    @brief Function to configure the route to the key path of the server.

    @param dmserver_servconn_pt s: Reference to server conn. structure.
    @param const char * key_path: Key path char string reference.
*/
void __dmserver_sconn_set_keypath(dmserver_servconn_pt s, const char * skey_path){
    strncpy(s->sssl_keypath, skey_path, DEFAULT_SCONN_KEYPATHLEN);
    s->sssl_keypath[DEFAULT_SCONN_KEYPATHLEN - 1] = '\0';
}