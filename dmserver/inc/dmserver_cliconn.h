/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMSERVER_CLICONN_HEADER
#define _DMSERVER_CLICONN_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "dmserver_hdrs.h"

/* ---- Defines  -------------------------------------------------- */
#define DEFAULT_CCONN_RBUFFERLEN 1024
#define DEFAULT_CCONN_WBUFFERLEN 1024

/* ---- Enumerations: Cli state ----------------------------------- */
enum dmserver_cconn_state{
    DMSERVER_CLIENT_UNABLE,
    DMSERVER_CLIENT_STANDBY,
    DMSERVER_CLIENT_ESTABLISHING,
    DMSERVER_CLIENT_ESTABLISHED,
    DMSERVER_CLIENT_CLOSED
};

/* ---- Data structures ------------------------------------------- */
// Client location data structure for dmserver:
struct dmserver_cliloc{
    size_t th_pos;
    size_t wc_pos;
};

// Client connection data structure for dmserver:
struct dmserver_cliconn{
    // Location of client:
    struct dmserver_cliloc cloc;

    // Connection data of a client:
    int cfd;

    sa_family_t caddr_family;
    union{
        struct sockaddr_in c4;
        struct sockaddr_in6 c6;
    }caddr;

    // Connection data of TLS of a client:
    SSL * cssl;
    BIO * cbio;

    // Read/Write buffers of a clientS:
    char crbuffer[DEFAULT_CCONN_RBUFFERLEN];
    pthread_mutex_t crlock;
    size_t crlen;

    char cwbuffer[DEFAULT_CCONN_WBUFFERLEN];
    pthread_mutex_t cwlock;
    size_t cwlen;

    // Client state:
    enum dmserver_cconn_state cstate;

    // Timeout ctl:
    time_t clastt;
};

/* ---- Data types ------------------------------------------------ */
// DMServer client & client location datatype:
typedef struct dmserver_cliconn dmserver_cliconn_t;
typedef dmserver_cliconn_t * dmserver_cliconn_pt;

typedef struct dmserver_cliloc dmserver_cliloc_t;
typedef dmserver_cliloc_t * dmserver_cliloc_pt;

/* ---- INTERNAL - Static functions prototypes -------------------- */
bool _dmserver_cconn_init(struct dmserver_cliconn * c);
bool _dmserver_cconn_deinit(struct dmserver_cliconn * c);
bool _dmserver_cconn_set(struct dmserver_cliconn * c, dmserver_cliloc_pt cloc, int cfd, struct sockaddr_storage * caddr, SSL * cssl);
bool _dmserver_cconn_reset(struct dmserver_cliconn * c);
bool _dmserver_cconn_checktimeout(struct dmserver_cliconn * c, time_t timeout_sec);

#endif