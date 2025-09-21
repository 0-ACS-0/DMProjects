/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMSERVER_CALLBACK_HEADER
#define _DMSERVER_CALLBACK_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "_dmserver_hdrs.h"
#include "_dmserver_cliconn.h"

/* ---- Data structures ------------------------------------------- */
// DMServer callbacks reference data structure:
struct dmserver_callback{
    // Callbacks available:
    void (*on_client_connect)(dmserver_cliconn_pt cli);
    void (*on_client_disconnect)(dmserver_cliconn_pt cli);
    void (*on_client_timeout)(dmserver_cliconn_pt cli);
    void (*on_client_rcv)(dmserver_cliconn_pt cli);
    void (*on_client_snd)(dmserver_cliconn_pt cli);
};

/* ---- Data types ------------------------------------------------ */
// Callback structure & callback configuration structure:
typedef struct dmserver_callback dmserver_callback_t;
typedef dmserver_callback_t * dmserver_callback_pt;

typedef dmserver_callback_t dmserver_callback_conf_t;
typedef dmserver_callback_pt dmserver_callback_conf_pt;

/* ---- INTERNAL - Static functions prototypes -------------------- */
// Callbacks setters functions:
void __dmserver_setcb_onclientconnect(dmserver_callback_pt cb, void (*on_client_connect)(dmserver_cliconn_pt));
void __dmserver_setcb_onclientdisconnect(dmserver_callback_pt cb, void (*on_client_disconnect)(dmserver_cliconn_pt));
void __dmserver_setcb_onclienttimeout(dmserver_callback_pt cb, void (*on_client_timeout)(dmserver_cliconn_pt));
void __dmserver_setcb_onclientrcv(dmserver_callback_pt cb, void (*on_client_rcv)(dmserver_cliconn_pt));
void __dmserver_setcb_onclientsnd(dmserver_callback_pt cb, void (*on_client_snd)(dmserver_cliconn_pt));

#endif