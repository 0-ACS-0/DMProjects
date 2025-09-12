/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMSERVER_SERVCONN_HEADER
#define _DMSERVER_SERVCONN_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "dmserver_hdrs.h"

/* ---- Defines  -------------------------------------------------- */
#define DEFAULT_SCONN_SPORT 8080
#define DEFAULT_SCONN_SFAMILY AF_INET
#define DEFAULT_SCONN_SSLENABLE false
#define DEFAULT_SCONN_CERTPATHLEN 128
#define DEFAULT_SCONN_CERTPATHVAL "./certs/server.crt"
#define DEFAULT_SCONN_KEYPATHLEN 128
#define DEFAULT_SCONN_KEYPATHVAL "./certs/server.key"

/* ---- Data structures ------------------------------------------- */
// Server connection data structre for dmserver:
struct dmserver_servconn{
    // Connection data of the server:
    int sfd;
    int sport;
    sa_family_t ssafamily;
    bool ss6only;
    union{
        struct sockaddr_in s4;
        struct sockaddr_in6 s6;
    }saddr;

    // Secure connection data of the server (including certificate and key paths):
    bool sssl_enable;
    const SSL_METHOD * sssl_method;
    SSL_CTX * sssl_ctx;
    char sssl_certpath[DEFAULT_SCONN_CERTPATHLEN];
    char sssl_keypath[DEFAULT_SCONN_KEYPATHLEN];
};

// Server connection data structure for configuration:
struct dmserver_servconn_conf{
    int sport;
    sa_family_t ssa_family;
    bool sipv6_only;
    bool stls_enable;
    char * scert_path;
    char * skey_path;
};

/* ---- Data types ------------------------------------------------ */
typedef struct dmserver_servconn dmserver_servconn_t;
typedef dmserver_servconn_t * dmserver_servconn_pt;

typedef struct dmserver_servconn_conf dmserver_servconn_conf_t;
typedef dmserver_servconn_conf_t * dmserver_servconn_conf_pt;

/* ---- INTERNAL - Static functions prototypes -------------------- */
// Server connection:
bool _dmserver_sconn_init(dmserver_servconn_pt s);
bool _dmserver_sconn_deinit(dmserver_servconn_pt s);
bool _dmserver_sconn_sslinit(dmserver_servconn_pt s);
bool _dmserver_sconn_ssldeinit(dmserver_servconn_pt s);
bool _dmserver_sconn_listen(dmserver_servconn_pt s);

// Server connection configuration:
void __dmserver_sconn_set_defaults(dmserver_servconn_pt s);
void __dmserver_sconn_set_port(dmserver_servconn_pt s, int sport);
void __dmserver_sconn_set_safamily(dmserver_servconn_pt s, sa_family_t sa_family);
void __dmserver_sconn_set_ipv6only(dmserver_servconn_pt s, bool sipv6_only);
void __dmserver_sconn_set_tls(dmserver_servconn_pt s, bool stls_enable);
void __dmserver_sconn_set_certpath(dmserver_servconn_pt s, const char * scert_path);
void __dmserver_sconn_set_keypath(dmserver_servconn_pt s, const char * skey_path);

#endif