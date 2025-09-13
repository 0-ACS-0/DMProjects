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
#include "dmserver_callback.h"
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
// DMServer data structure:
struct dmserver{
    dmserver_servconn_t sconn;
    dmserver_worker_t sworker;
    dmserver_callback_t scallback;
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
bool dmserver_conf_sconn(dmserver_pt dmserver, dmserver_servconn_conf_pt sconn_conf);
bool dmserver_conf_worker(dmserver_pt dmserver, dmserver_worker_conf_pt worker_conf);
bool dmserver_conf_cconn(dmserver_pt dmserver, dmserver_cliconn_conf_pt cconn_conf);

// Configuration - Set callbacks:
bool dmserver_set_cb(dmserver_pt dmserver, dmserver_callback_conf_pt callback_conf);

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