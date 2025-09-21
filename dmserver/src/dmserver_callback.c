/*

*/

/* ---- Library --------------------------------------------------- */
#include "../inc/_dmserver_callback.h"

/* ---- INTERNAL - Functions implementation ----------------------- */
// ======== Callbacks setters:

/*
    @brief Function to set a callback function when a client connects to the server.

    @param dmserver_callback_pt cb: Reference to callbacks struct.
    @param void (*on_client_connect)(dmserver_cliconn_pt): Reference to callback function.
*/
void __dmserver_setcb_onclientconnect(dmserver_callback_pt cb, void (*on_client_connect)(dmserver_cliconn_pt)){
    // Callback assignation:
    cb->on_client_connect = on_client_connect;
}

/*
    @brief Function to set a callback function when a client disconnects to the server.

    @param dmserver_callback_pt cb: Reference to callbacks struct.
    @param void (*on_client_disconnect)(dmserver_cliconn_pt): Reference to callback function.
*/
void __dmserver_setcb_onclientdisconnect(dmserver_callback_pt cb, void (*on_client_disconnect)(dmserver_cliconn_pt)){
    // Callback assignation:
    cb->on_client_disconnect = on_client_disconnect;
}

/*
    @brief Function to set a callback function when a client timeout.

    @param dmserver_callback_pt cb: Reference to callbacks struct.
    @param void (*on_client_timeout)(dmserver_cliconn_pt): Reference to callback function.
*/
void __dmserver_setcb_onclienttimeout(dmserver_callback_pt cb, void (*on_client_timeout)(dmserver_cliconn_pt)){
    // Callback assignation:
    cb->on_client_timeout = on_client_timeout;
}

/*
    @brief Function to set a callback function when received data from client.

    @param dmserver_callback_pt cb: Reference to callbacks struct.
    @param void (*on_client_rcv)(dmserver_cliconn_pt): Reference to callback function.
*/
void __dmserver_setcb_onclientrcv(dmserver_callback_pt cb, void (*on_client_rcv)(dmserver_cliconn_pt)){
    // Callback assignation:
    cb->on_client_rcv = on_client_rcv;
}

/*
    @brief Function to set a callback function when sent data to client.

    @param dmserver_callback_pt cb: Reference to callbacks struct.
    @param void (*on_client_snd)(dmserver_cliconn_pt): Reference to callback function.
*/
void __dmserver_setcb_onclientsnd(dmserver_callback_pt cb, void (*on_client_snd)(dmserver_cliconn_pt)){
    // Callback assignation:
    cb->on_client_snd = on_client_snd;
}