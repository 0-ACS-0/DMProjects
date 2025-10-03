/*

*/

/* ---- Library --------------------------------------------------- */
#include "../inc/_dmserver_cliconn.h"


/* ---- INTERNAL - Functions implementation ----------------------- */
// ======== General use:
/*
    @brief Function to initialize a client connection slot.
    @note Because this function will initialize completly a client slot, will initialize it with
    defaults buffers size.

    @param struct dmserver_cliconn *c: Reference to client slot of dmserver-worker.

    @retval true: Initialization succeeded.
    @retval false: Initialization failed.
*/
bool _dmserver_cconn_init(struct dmserver_cliconn * c){
    // Reference & state check:
    if (!c) return false;
    if (c->cstate != DMSERVER_CLIENT_UNABLE) return false;

    // Initialize conection data:
    c->cfd = -1;
    c->caddr_family = AF_UNSPEC;

    // Initialize read/write buffers:
    __dmserver_cconn_set_defaults(c);
    if(!__dmserver_cconn_buf_alloc(c)) return false; 

    // Initialize mutex logic:
    if(pthread_mutex_init(&c->crlock, NULL)) {
        __dmserver_cconn_buf_dealloc(c);
        return false;
    }

    if(pthread_mutex_init(&c->cwlock, NULL)) {
        pthread_mutex_destroy(&c->crlock);
        return false;
    }


    // Initialize state:
    c->cstate = DMSERVER_CLIENT_STANDBY;
    return true;
}

/*
    @brief Function to deinitialize a client slot.

    @param struct dmerver_cliconn *c: Reference to client slot of dmserver-worker.

    @retval true: Deinitialization succeeded.
    @retval false: Deinitialization failed.
*/
bool _dmserver_cconn_deinit(struct dmserver_cliconn * c){
    // Reference & state check:
    if (!c) return false;
    if (c->cstate == DMSERVER_CLIENT_UNABLE) return false;
    if ((c->cstate != DMSERVER_CLIENT_CLOSED) && (c->cstate != DMSERVER_CLIENT_STANDBY)) return false;

    // Deinitialize connection data:
    c->cfd = -1;
    c->caddr_family = AF_UNSPEC;

    // Deinitialize read/write buffers:
    if(!__dmserver_cconn_buf_dealloc(c)) return false; 

    // Deinitialize mutex logic:
    pthread_mutex_destroy(&c->crlock);
    pthread_mutex_destroy(&c->cwlock);

    // Deinitialize state:
    c->cstate = DMSERVER_CLIENT_UNABLE;
    return true;
}

/*
    @brief Function to set all the connection data to a client slot.
    @note: Before setting all the information given, a full connection (accept + ssl) should be stablished 
    in order to copy correctly all the client socket file descripotr, address structure and ssl reference.

    @param struct dmerver_cliconn *c: Reference to client slot of dmserver-worker.
    @param int cfd: client-socket file descriptor.
    @param struct sockaddr_storage *caddr: Client connection address.
    @param SSL * cssl: Reference to client secure socket layer.

    @retval true: Set succeeded.
    @retval false: Set failed.
*/
bool _dmserver_cconn_set(struct dmserver_cliconn * c, dmserver_cliloc_pt cloc, int cfd, struct sockaddr_storage * caddr, SSL * cssl){
    // References & values & state checks:
    if (!c || (cfd < 0) || !caddr) return false;
    if (c->cstate != DMSERVER_CLIENT_STANDBY) return false;

    // Set the client location:
    c->cloc.th_pos = cloc->th_pos;
    c->cloc.wc_pos = cloc->wc_pos;

    // Set socket file descriptor:
    c->cfd = cfd;

    // Set the address info and connection data into client (by the given IP protocl):
    c->caddr_family = caddr->ss_family;
    if (caddr->ss_family == AF_INET){
        struct sockaddr_in * addr4 = (struct sockaddr_in *)caddr;
        c->caddr.c4 = *addr4;
    } else if (caddr->ss_family == AF_INET6){
        struct sockaddr_in6 * addr6 = (struct sockaddr_in6 *)caddr;
        if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
            c->caddr.c4.sin_family = AF_INET;
            c->caddr.c4.sin_port = addr6->sin6_port;
            memcpy(&c->caddr.c4.sin_addr, &addr6->sin6_addr.s6_addr[12], 4);
        }
        else c->caddr.c6 = *addr6;
    }

    // Set ssl session reference:
    c->cssl = cssl;

    // Set last time timeout to now:
    c->clastt = time(NULL);

    // Set established state:
    c->cstate = DMSERVER_CLIENT_ESTABLISHED;
    return true;
}

/*
    @brief Function to reset a client slot.
    @note: Before reseting the information in the client slot, the connection must be closed correctly
    outside this function, or it will lead to important leaks!

    @param struct dmerver_cliconn *c: Reference to client slot of dmserver-worker.

    @retval true: Reset succeeded.
    @retval false: Reset failed.
*/
bool _dmserver_cconn_reset(struct dmserver_cliconn * c){
    // Reference & state check:
    if (!c) return false;
    if (c->cstate != DMSERVER_CLIENT_CLOSED) return false;

    // Reset location data:
    c->cloc.th_pos = 0;
    c->cloc.wc_pos = 0;

    // Reset connection data:
    c->cssl = NULL;
    c->cfd = -1;
    
    c->caddr_family = AF_UNSPEC;
    memset(&c->caddr, 0, sizeof(c->caddr));
    c->clastt = 0;

    // Reset read/write buffers:
    memset(c->crbuffer, 0, c->crbuffer_size);
    c->crlen = 0;

    memset(c->cwbuffer, '\0', c->cwbuffer_size);
    c->cwlen = 0;

    // Reset state:
    c->cstate = DMSERVER_CLIENT_STANDBY;

    return true;
}

/*
    @brief Function to check the client timeout field.

    @param struct dmserver_cliconn * c: Reference to client to check.

    @retval true: No timeout nor fail occured.
    @retval false: Error or timeout occured.
*/
bool _dmserver_cconn_checktimeout(struct dmserver_cliconn * c, time_t timeout_sec){
    // Reference check:
    if (!c || (timeout_sec == 0)) return false;

    // Timeout check: 
    if ((time(NULL) - c->clastt) > timeout_sec) return false;
    return true;
}

// ======== Configuration:
/*
    @brief Function to allocate the buffers memory of the client.

    @param dmserver_cliconn_pt c: Reference to the client structure.

    @retval true: Allocation succeeded.
    @retval false: Allocation failed.
*/
bool __dmserver_cconn_buf_alloc(dmserver_cliconn_pt c){
    // Reference check:
    if (!c) return false;

    // Allocate and assign memory for the read/write client buffers:
    c->crbuffer = calloc(c->crbuffer_size, sizeof(char));
    if (!c->crbuffer) {
        __dmserver_cconn_buf_dealloc(c);
        return false;
    }

    c->cwbuffer = calloc(c->cwbuffer_size, sizeof(char));
    if (!c->crbuffer) {
        __dmserver_cconn_buf_dealloc(c);
        return false;
    }
    return true;
}

/*
    @brief Function to deallocate the buffers memory of the client.

    @param dmserver_cliconn_pt c: Reference to the client structure.

    @retval true: Dellocation succeeded.
    @retval false: Deallocation failed. 
*/
bool __dmserver_cconn_buf_dealloc(dmserver_cliconn_pt c){
    // Reference check:
    if (!c) return false;

    // Deallocate buffers memory (if previously allocated):
    if (c->crbuffer) free(c->crbuffer);
    if (c->cwbuffer) free(c->cwbuffer);
    return true;
}

/*
    @brief Function to set the read and write client biffers to default size.

    @param dmserver_cliconn_pt c: Reference to client structure.
*/
void __dmserver_cconn_set_defaults(dmserver_cliconn_pt c){
    c->crbuffer_size = DEFAULT_CCONN_RBUFFERLEN;
    c->cwbuffer_size = DEFAULT_CCONN_WBUFFERLEN;
}

/*
    @brief Function to set the size of the client read buffer.
    @note: Allocation must be done to these changes take effect (deallocate before a new
    allocation to avoid memory leaks).
    
    @param dmserver_cliconn_pt c: Reference to client structure.
*/
void __dmserver_cconn_set_creadbuffer(dmserver_cliconn_pt c, size_t crbuf_size){
    c->crbuffer_size = crbuf_size;
}

/*
    @brief Function to set the size of the client write buffer.
    @note: Allocation must be done to these changes take effect (deallocate before a new
    allocation to avoid memory leaks).
    
    @param dmserver_cliconn_pt c: Reference to client structure.
*/
void __dmserver_cconn_set_cwritebuffer(dmserver_cliconn_pt c, size_t cwbuf_size){
    c->cwbuffer_size = cwbuf_size;
}