/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMSERVER_CLICONN_HEADER
#define _DMSERVER_CLICONN_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "dmserver_hdrs.h"

/* ---- Defines  -------------------------------------------------- */
#define DEFAULT_CCONN_RBUFFERLEN 4096
#define DEFAULT_CCONN_WBUFFERLEN 4096

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

    // Read/Write buffers of a clientS:DEFAULT_CCONN_RBUFFERLEN]
    size_t crbuffer_size;
    char * crbuffer;
    pthread_mutex_t crlock;
    size_t crlen;

    size_t cwbuffer_size;
    char * cwbuffer;
    pthread_mutex_t cwlock;
    size_t cwlen;

    // Client state:
    enum dmserver_cconn_state cstate;

    // Timeout ctl:
    time_t clastt;
};

// Client buffers length configuration:
struct dmserver_cliconn_conf{
    size_t cread_buffer_size;
    size_t cwrite_buffer_size;
};


/* ---- Data types ------------------------------------------------ */
// DMServer client & client location datatype:
typedef struct dmserver_cliconn dmserver_cliconn_t;
typedef dmserver_cliconn_t * dmserver_cliconn_pt;

typedef struct dmserver_cliloc dmserver_cliloc_t;
typedef dmserver_cliloc_t * dmserver_cliloc_pt;

// DMServer client buffers configuration structure:
typedef struct dmserver_cliconn_conf dmserver_cliconn_conf_t;
typedef dmserver_cliconn_conf_t * dmserver_cliconn_conf_pt;

/* ---- INTERNAL - Static functions prototypes -------------------- */
// Client connection:
bool _dmserver_cconn_init(dmserver_cliconn_pt c);
bool _dmserver_cconn_deinit(dmserver_cliconn_pt c);
bool _dmserver_cconn_set(dmserver_cliconn_pt c, dmserver_cliloc_pt cloc, int cfd, struct sockaddr_storage * caddr, SSL * cssl);
bool _dmserver_cconn_reset(dmserver_cliconn_pt c);
bool _dmserver_cconn_checktimeout(dmserver_cliconn_pt c, time_t timeout_sec);

// Client connection configuration:
bool __dmserver_cconn_buf_alloc(dmserver_cliconn_pt c);
bool __dmserver_cconn_buf_dealloc(dmserver_cliconn_pt c);
void __dmserver_cconn_set_defaults(dmserver_cliconn_pt c);
void __dmserver_cconn_set_creadbuffer(dmserver_cliconn_pt c, size_t crbuf);
void __dmserver_cconn_set_cwritebuffer(dmserver_cliconn_pt c, size_t cwbuf);

#endif