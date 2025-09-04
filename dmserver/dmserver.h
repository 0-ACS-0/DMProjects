/*
    ============
      DMSERVER
    ============

    Dmserver is a simple yet well-balanced server utility designed for Unix systems (same as dmlogger, yup).
    
    ... TODO ...

    --------
    Author: Antonio Carretero Sahuquillo
    Date: 2025-08-29

    Licensed under the GNU General Public License v3.0 (GPLv3).
    See https://www.gnu.org/licenses/gpl-3.0.html for details.
*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMSERVER_HEADER
#define _DMSERVER_HEADER
#include <bits/sockaddr.h>
#ifndef _GNU_SOURCE 
#define _GNU_SOURCE
#endif

/* ---- Libraries ------------------------------------------------- */
// Standard:
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Unix:
#include <unistd.h>

// Character strings manipulation:
#include <string.h>

// Error codes to identify error conditions:
#include <errno.h>

// Threads:
#include <pthread.h>

// Network:
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 

// Events I/O:
#include <sys/epoll.h>
#include <sys/eventfd.h>

// OpenSSL (TLS):
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

// Signals:
#include <signal.h>

// Time for timeout:
#include <time.h>

// Logging system (from my dm projects :D)
#include "dmlogger.h"

/* ---- Defines & macros ------------------------------------------ */
#define DEFAULT_SCONN_SPORT 1024
#define DEFAULT_SCONN_SFAMILY AF_INET
#define DEFAULT_SCONN_CERTPATHLEN 128
#define DEFAULT_SCONN_CERTPATHVAL "./certs/server.crt"
#define DEFAULT_SCONN_KEYPATHLEN 128
#define DEFAULT_SCONN_KEYPATHVAL "./certs/server.key"

#define DEFAULT_CCONN_RBUFFERLEN 1024
#define DEFAULT_CCONN_WBUFFERLEN 1024

#define DEFAULT_WORKER_SUBTHREADS 8
#define DEFAULT_WORKER_CLISPERSTH 200
#define DEFAULT_WORKER_CLITIMEOUT 120

/* ---- Enumerations ---------------------------------------------- */
// Clients state:
enum dmserver_cconn_state{
    DMSERVER_CLIENT_UNABLE,
    DMSERVER_CLIENT_STANDBY,
    DMSERVER_CLIENT_ESTABLISHED,
    DMSERVER_CLIENT_CLOSED
};

// Server state:
enum dmserver_state{
    DMSERVER_STATE_INITIALIZED,
    DMSERVER_STATE_OPENED,    
    DMSERVER_STATE_RUNNING,         
    DMSERVER_STATE_STOPPING,        
    DMSERVER_STATE_STOPPED,
    DMSERVER_STATE_CLOSED,           
};

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
    const SSL_METHOD * sssl_method;
    SSL_CTX * sssl_ctx;
    char sssl_certpath[DEFAULT_SCONN_CERTPATHLEN];
    char sssl_keypath[DEFAULT_SCONN_KEYPATHLEN];
};

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

    // Read/Write buffers of a clientS:
    char crbuffer[DEFAULT_CCONN_RBUFFERLEN];
    pthread_mutex_t crlock;
    size_t crlen;
    size_t croff;

    char cwbuffer[DEFAULT_CCONN_WBUFFERLEN];
    pthread_mutex_t cwlock;
    size_t cwlen;
    size_t cwoff;

    // Client state:
    enum dmserver_cconn_state cstate;

    // Timeout ctl:
    time_t clastt;
};

// Workers data structure for dmserver:
struct dmserver_worker{
    // Threads (workers) data:
    pthread_t wmainth;
    int wmainepfd;
    pthread_t wsubth[DEFAULT_WORKER_SUBTHREADS];
    int wsubepfd[DEFAULT_WORKER_SUBTHREADS];

    // Clients placeholder for each sub-thread:
    struct dmserver_cliconn wcclis[DEFAULT_WORKER_SUBTHREADS][DEFAULT_WORKER_CLISPERSTH];
    size_t wccount[DEFAULT_WORKER_SUBTHREADS];
    time_t wctimeout;

    // Callbacks available:
    void (*on_client_connect)(void * args);
    void (*on_client_disconnect)(void * args);
    void (*on_client_timeout)(void * args);
    void (*on_client_rcv)(void * args);
    void (*on_client_snd)(void * args);
};

// DMServer data structure:
struct dmserver{
    struct dmserver_servconn sconn;
    struct dmserver_worker sworker;
    dmlogger_pt slogger;

    enum dmserver_state sstate;
};

/* ---- Data types ------------------------------------------------ */
// DMServer datatype:
typedef struct dmserver dmserver_t;
typedef dmserver_t * dmserver_pt;

// DMServer client location datatype:
typedef struct dmserver_cliloc dmserver_cliloc_t;
typedef dmserver_cliloc_t * dmserver_cliloc_pt;

/* ---- Functions prototypes -------------------------------------- */
// Initialization / Deinitialization:
void dmserver_init(dmserver_pt * dmserver);
void dmserver_deinit(dmserver_pt * dmserver);

// Configuration - General:
bool dmserver_conf_port(dmserver_pt dmserver, int port);
bool dmserver_conf_safamily(dmserver_pt dmserver, sa_family_t safamily);
bool dmserver_conf_ipv6only(dmserver_pt dmserver, bool ipv6only);
bool dmserver_conf_certpath(dmserver_pt dmserver, const char * certpath);
bool dmserver_conf_keypath(dmserver_pt dmserver, const char * keypath);

// Configuration - Callbacks:
bool dmserver_setcb_onclientconnect(dmserver_pt dmserver, void (*on_client_connect)(void *));
bool dmserver_setcb_onclientdisconnect(dmserver_pt dmserver, void (*on_client_disconnect)(void *));
bool dmserver_setcb_onclienttimeout(dmserver_pt dmserver, void (*on_client_timeout)(void *));
bool dmserver_setcb_onclientrcv(dmserver_pt dmserver, void (*on_client_rcv)(void *));
bool dmserver_setcb_onclientsnd(dmserver_pt dmserver, void (*on_client_snd)(void *));

// Open / Run / Stop / Close:
bool dmserver_open(dmserver_pt dmserver);
bool dmserver_run(dmserver_pt dmserver);
bool dmserver_stop(dmserver_pt dmserver);
bool dmserver_close(dmserver_pt dmserver);

// Broadcast / Unicast:
bool dmserver_broadcast(dmserver_pt dmserver, const char * bcdata);
bool dmserver_unicast(dmserver_pt dmserver, dmserver_cliloc_pt dmcliloc, const char * ucdata);

#endif