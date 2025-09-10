/*
    ============
      DMSERVER
    ============

    Dmserver is a simple (or not so simple) yet well-balanced server utility designed for Unix systems (same as dmlogger, yup).
    
    It is a TCP/IP server, which uses TLSv1.3 by default and that handles clients in a efficiente/balanced way. 
    With a simple architecture where a thread handle clients connections, and another bunch handle all thoses clients
    multiplexed by UNIX defined functionality.

    It is non-blocking, which means all the server processes are running on different threads, 
    implementing user-defined callbacks for customized application layer. 
    Also providing simple configuration options to select how the server side will behave in a low level basis.

    --------
    Author: Antonio Carretero Sahuquillo
    Date: 2025-08-29

    Licensed under the GNU General Public License v3.0 (GPLv3).
    See https://www.gnu.org/licenses/gpl-3.0.html for details.
*/

/* ---- Header guard ---------------------------------------------- */
#ifndef DMSERVER_HEADER
#define DMSERVER_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "dmserver_hdrs.h"
#include "dmserver_servconn.h"
#include "dmserver_worker.h"

/* ---- Enumerations ---------------------------------------------- */
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
// DMServer callbacks reference data structure:
struct dmserver_callback{
    // Callbacks available:
    void (*on_client_connect)(struct dmserver_cliconn * cli);
    void (*on_client_disconnect)(struct dmserver_cliconn * cli);
    void (*on_client_timeout)(struct dmserver_cliconn * cli);
    void (*on_client_rcv)(struct dmserver_cliconn * cli);
    void (*on_client_snd)(struct dmserver_cliconn * cli);
};

// DMServer data structure:
struct dmserver{
    struct dmserver_servconn sconn;
    struct dmserver_worker sworker;
    struct dmserver_callback scallback;
    dmlogger_pt slogger;

    enum dmserver_state sstate;
};

/* ---- Data types ------------------------------------------------ */
// DMServer datatype:
typedef struct dmserver dmserver_t;
typedef dmserver_t * dmserver_pt;

/* ---- Functions prototypes -------------------------------------- */
// Initialization / Deinitialization:
void dmserver_init(dmserver_pt * dmserver);
void dmserver_deinit(dmserver_pt * dmserver);

// Configuration - General:
bool dmserver_conf_port(dmserver_pt dmserver, int port);
bool dmserver_conf_safamily(dmserver_pt dmserver, sa_family_t safamily);
bool dmserver_conf_ipv6only(dmserver_pt dmserver, bool ipv6only);
bool dmserver_conf_tlsenable(dmserver_pt dmserver, bool tlsv13en);
bool dmserver_conf_certpath(dmserver_pt dmserver, const char * certpath);
bool dmserver_conf_keypath(dmserver_pt dmserver, const char * keypath);

// Configuration - Callbacks:
bool dmserver_setcb_onclientconnect(dmserver_pt dmserver, void (*on_client_connect)(dmserver_cliconn_pt));
bool dmserver_setcb_onclientdisconnect(dmserver_pt dmserver, void (*on_client_disconnect)(dmserver_cliconn_pt));
bool dmserver_setcb_onclienttimeout(dmserver_pt dmserver, void (*on_client_timeout)(dmserver_cliconn_pt));
bool dmserver_setcb_onclientrcv(dmserver_pt dmserver, void (*on_client_rcv)(dmserver_cliconn_pt));
bool dmserver_setcb_onclientsnd(dmserver_pt dmserver, void (*on_client_snd)(dmserver_cliconn_pt));

// Open / Run / Stop / Close:
bool dmserver_open(dmserver_pt dmserver);
bool dmserver_run(dmserver_pt dmserver);
bool dmserver_stop(dmserver_pt dmserver);
bool dmserver_close(dmserver_pt dmserver);

// Broadcast / Unicast / Disconnect:
bool dmserver_broadcast(dmserver_pt dmserver, dmserver_cliloc_pt bexclude, const char * bcdata);
bool dmserver_unicast(dmserver_pt dmserver, dmserver_cliloc_pt dmcliloc, const char * ucdata);
bool dmserver_disconnect(dmserver_pt dmserver, dmserver_cliloc_pt dmcliloc);

#endif