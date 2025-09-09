/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMSERVER_SERVCONN_HEADER
#define _DMSERVER_SERVCONN_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "dmserver_hdrs.h"

/* ---- Defines  -------------------------------------------------- */
#define DEFAULT_SCONN_SPORT 1024
#define DEFAULT_SCONN_SFAMILY AF_INET
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
    sa_family_t sfamily;
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

/* ---- INTERNAL - Static functions prototypes -------------------- */
bool _dmserver_sconn_init(struct dmserver_servconn * s);
bool _dmserver_sconn_deinit(struct dmserver_servconn * s);
bool _dmserver_sconn_sslinit(struct dmserver_servconn * s);
bool _dmserver_sconn_ssldeinit(struct dmserver_servconn * s);
bool _dmserver_sconn_listen(struct dmserver_servconn * s);

#endif