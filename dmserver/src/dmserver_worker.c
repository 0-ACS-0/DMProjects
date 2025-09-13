/*

*/

/* ---- Library --------------------------------------------------- */
#include "../inc/dmserver_worker.h"
#include "../inc/dmserver.h"

/* ---- Helper functions implementation prototypes ---------------- */
static void _dmserver_helper_smanager(dmserver_pt dmserver);
static bool _dmserver_helper_csslhandshake(dmserver_pt dmserver, dmserver_cliconn_pt c);
static bool _dmserver_helper_cctimeout(dmserver_pt dmserver, dmserver_cliconn_pt dmclient);
static bool _dmserver_helper_ccread(dmserver_pt dmserver, dmserver_cliconn_pt dmclient, size_t dmthindex, struct epoll_event * evs, size_t evindex);
static bool _dmserver_helper_ccwrite(dmserver_pt dmserver, dmserver_cliconn_pt dmclient, size_t dmthindex, struct epoll_event * evs, size_t evindex);




/* ---- INTERNAL - Functions implementation ----------------------- */
// ======== Allocators:
/*
    @brief Function to allocate all the configurable worker memory (also create the epoll infrastructure). 

    @param dmserver_worker_pt w: Worker struct reference.

    @retval true: Allocation succeeded.
    @retval false: Allocation failed.
*/
bool __dmserver_worker_alloc(dmserver_worker_pt w){
    // Reference check:
    if (!w) return false;

    // Main thread epoll:
    w->wmainepfd = epoll_create1(0);
    if (w->wmainepfd == -1) {__dmserver_worker_dealloc(w); return false;}

    // Allocation for subthreads, subthreads epoll:
    w->wsubth = calloc(w->wth_subthreads, sizeof(pthread_t));
    if (!w->wsubth) {__dmserver_worker_dealloc(w); return false;}
    w->wsubepfd = calloc(w->wth_subthreads, sizeof(int));
    if (!w->wsubepfd) {__dmserver_worker_dealloc(w); return false;}

    // Allocation for clients queue (including counters):
    w->wccount = calloc(w->wth_subthreads, sizeof(size_t));
    if (!w->wccount) {__dmserver_worker_dealloc(w); return false;}
    w->wcclis = calloc(w->wth_subthreads, sizeof(dmserver_cliconn_pt));
    if (!w->wcclis) {__dmserver_worker_dealloc(w); return false;}
    for (size_t i = 0; i < w->wth_subthreads; i++){
        w->wcclis[i] = calloc(w->wth_clispersth, sizeof(dmserver_cliconn_t));
        if (!w->wcclis[i]) {__dmserver_worker_dealloc(w); return false;}
        w->wsubepfd[i] = epoll_create1(0);
        if (w->wsubepfd[i] == -1) {__dmserver_worker_dealloc(w); return false;}
        for (size_t j = 0; j < w->wth_clispersth; j++){
            if (!_dmserver_cconn_init(&w->wcclis[i][j])) {__dmserver_worker_dealloc(w); return false;}
        }
    }

    return true;
}

/*
    @brief Function to deallocate all the configurable memory reserver for the worker.

    @param dmserver_worker_pt w: Worker reference.

    @retval true: Deallocation succeeded.
    @retval false: Deallocation failed.
*/
bool __dmserver_worker_dealloc(dmserver_worker_pt w){
    // Deallocation of clients queue data and subordinate threads epoll:
    for (size_t i = 0; i < w->wth_subthreads; i++){
        for (size_t j = 0; j < w->wth_clispersth; j++){
            if (!_dmserver_cconn_deinit(&w->wcclis[i][j])) return false;
        }
        if (w->wsubepfd[i] != -1) close(w->wsubepfd[i]);
        if (w->wcclis[i]) free(w->wcclis[i]);
    }
    if (w->wmainepfd != -1) close(w->wmainepfd);
    if (w->wcclis) free(w->wcclis);
    if (w->wccount) free(w->wccount);

    // Deallocation of the rest of reserved memory:
    if (w->wsubepfd) free(w->wsubepfd);
    if (w->wsubth) free(w->wsubth);

    return true;
}

// ======== Setters:
/*
    @brief Function to initialize the worker to its defaults values.
    @note: For this changes to take effect, the worker must be allocated afterwards, but
    remember to deallocate first to avoid memory leaks.

    @param dmserver_worker_t w: Reference to worker structure.
*/
void __dmserver_worker_set_defaults(dmserver_worker_pt w){
    // Set defaults number of threads, number of clients per subordinate thread and client timeout:
    w->wth_subthreads = DEFAULT_WORKER_SUBTHREADS;
    w->wth_clispersth = DEFAULT_WORKER_CLISPERSTH;
    w->wth_clistimeout = DEFAULT_WORKER_CLITIMEOUT;
}

/*
    @brief Function to set a specific number of subordinate workers threads.
    @note: For this changes to take effect, the worker must be allocated afterwards, but
    remember to deallocate first to avoid memory leaks.

    @param dmserver_worker_t w: Reference to worker structure.
    @param size_t wth_subthreads: Number of subordinate worker threads.
*/
void __dmserver_worker_set_subthreads(dmserver_worker_pt w, size_t wth_subthreads){
    w->wth_subthreads = wth_subthreads;
}

/*
    @brief Function to set a specific number of clients per subordinate worker thread.
    @note: For this changes to take effect, the worker must be allocated afterwards, but
    remember to deallocate first to avoid memory leaks.

    @param dmserver_worker_t w: Reference to worker structure.
    @param size_t wth_subthreads: Number of clients per subordinate worker threads.
*/
void __dmserver_worker_set_clispersth(dmserver_worker_pt w, size_t wth_clispersth){
    w->wth_clispersth = wth_clispersth;
}

/*
    @brief Function to set the clients maximum timeout without interaction with worker.
    @note: For this changes to take effect, the worker must be allocated afterwards, but
    remember to deallocate first to avoid memory leaks.

    @param dmserver_worker_t w: Reference to worker structure.
    @param size_t wth_subthreads: Number of seconds without interaction allowed.
*/
void __dmserver_worker_set_clistimeout(dmserver_worker_pt w, size_t wth_clistimeout){
    w->wth_clistimeout = wth_clistimeout;
}



// ======== Threads:
/*
    @brief Function that implements the main thread of dmserver.
    Accept connections and distribute clients to subordinated threads.

    @param void * args: Reference to the dmserver struct.

    @retval NULL. (always)
*/
void * _dmserver_worker_main(void * args){
    // Reference check and cast:
    if (!args) return NULL;
    dmserver_pt dmserver = (dmserver_pt)args;

    // Prepare the main thread epoll to optimize CPU usage:
    if (epoll_ctl(dmserver->sworker.wmainepfd, EPOLL_CTL_ADD, dmserver->sconn.sfd, &(struct epoll_event){.events=EPOLLIN|EPOLLET, .data.fd=dmserver->sconn.sfd}) < 0) 
        return NULL;
    struct epoll_event evs[SOMAXCONN];
    
    while (dmserver->sstate == DMSERVER_STATE_RUNNING){
        // Epoll wait for connection:
        int nfds = epoll_wait(dmserver->sworker.wmainepfd, evs, SOMAXCONN, 4000);
        if (nfds < 0  && (errno == EINTR)) continue;

        for (size_t i = 0; i < nfds; i++){
            // Server client connection manager:
            _dmserver_helper_smanager(dmserver);
        }
    }

    // Delete the server socket file descriptor from main thread epoll:
    epoll_ctl(dmserver->sworker.wmainepfd, EPOLL_CTL_DEL, dmserver->sconn.sfd, NULL);
    return NULL;
}

/*
    @brief Function that implements the subordinated threads of dmserver.
    Manage the clients data reception and transmission.

    @param void * args: Reference to the arguments structure.

    @retval NULL. (always)
*/
void * _dmserver_worker_sub(void * args){
    // Reference check and cast:
    if (!args) return NULL;
    dmserver_subthargs_t * dmargs = (dmserver_subthargs_t *)args;
    dmserver_pt dmserver = dmargs->dmserver;
    size_t dmthindex = dmargs->subthindex;

    // Subthread timeout worker to check periodically the clients timeouts:
    pthread_t ctimeout_th;
    if (pthread_create(&ctimeout_th, NULL, _dmserver_subworker_timeout, args)) return NULL;

    // Prepare the subordinate thread epoll to optimize CPU usage:
    struct epoll_event evs[dmserver->sworker.wth_clispersth];

    while (dmserver->sstate == DMSERVER_STATE_RUNNING){
        // Epoll wait for events:
        int nfds = epoll_wait(dmserver->sworker.wsubepfd[dmthindex], evs, dmserver->sworker.wth_clispersth, 4000);
        if ((nfds < 0)  || (errno == EINTR)) {continue;}

        for (size_t i = 0; i < nfds; i++){
            // Obtain the pointer and check the state of the client that generated the event:
            dmserver_cliconn_pt dmclient = evs[i].data.ptr;
            if (!dmclient || ((dmclient->cstate != DMSERVER_CLIENT_ESTABLISHED) && (dmclient->cstate != DMSERVER_CLIENT_ESTABLISHING))) continue;

            // Connection stages check:
            if(!_dmserver_helper_csslhandshake(dmserver, dmclient)) continue;

            // Handle read:
            if(!_dmserver_helper_ccread(dmserver, dmclient, dmthindex, evs, i)) continue;

            // Handle write:
            if(!_dmserver_helper_ccwrite(dmserver, dmclient, dmthindex, evs, i)) continue;
        }
    }

    // Kill the timeout checker thread:
    pthread_cancel(ctimeout_th);
    pthread_join(ctimeout_th, NULL);

    // Completly closes the clients connections at thread exit:
    for (size_t i = 0; i < dmserver->sworker.wth_clispersth; i++){
        dmserver_disconnect(dmserver, &(dmserver_cliloc_t){.th_pos=dmthindex, .wc_pos=i});
    }

    return NULL;
}

/*
    @brief Function that implements the subthread to check clients timeout periodically of the
    subordinate worker creator.
    @note: The argument memory liberation happens here.

    @param void * args: Reference to the arguments structure.

    @retval NULL. (always)
*/
void * _dmserver_subworker_timeout(void * args){
    // Reference check and cast:
    if (!args) return NULL;
    dmserver_subthargs_t * dmargs = (dmserver_subthargs_t *)args;
    dmserver_pt dmserver = dmargs->dmserver;
    size_t dmthindex = dmargs->subthindex;

    // Free args memory:
    free(args);

    while (true){
        // Check timeout of the subworker clients:
        for (size_t i = 0; i < dmserver->sworker.wth_clispersth; i++){
            _dmserver_helper_cctimeout(dmserver, &dmserver->sworker.wcclis[dmthindex][i]); 
        }

        // CPU sleep to avoid overload:
        sleep(dmserver->sworker.wth_clistimeout / 8);
    }

    return NULL;
}




/* ---- STATIC INTERNAL - Helper functions implementation --------- */
/*
    @brief Helper function that implements the server connection, distribution and management of an
    incoming client connection.
    
    @param dmserver_pt server: Reference to the server struct.

    @retval None.
*/
static void _dmserver_helper_smanager(dmserver_pt dmserver){
    // Accept TCP connection:
    int temp_cfd;
    struct sockaddr_storage temp_caddr;
    socklen_t temp_caddrlen = sizeof(temp_caddr);
    temp_cfd = accept4(dmserver->sconn.sfd, (struct sockaddr *)&temp_caddr, &temp_caddrlen, SOCK_NONBLOCK);
    if (temp_cfd < 0) return;

    // Distribute client to the less populated subordinate thread and the next free slot (just find the location in the client matrix):
    size_t temp_thindex = 0;
    for (size_t i = 0; i < dmserver->sworker.wth_subthreads; i++){
        if (dmserver->sworker.wccount[i] < dmserver->sworker.wccount[temp_thindex]) temp_thindex = i;
    }
    size_t temp_cindex = 0;
    for (size_t i = 0; i < dmserver->sworker.wth_clispersth; i++){
        if (dmserver->sworker.wcclis[temp_thindex][i].cstate == DMSERVER_CLIENT_STANDBY) {temp_cindex = i; break;}
    }

    // Check if server capacity is full:
    dmserver_cliconn_pt dmclient = &dmserver->sworker.wcclis[temp_thindex][temp_cindex];
    if ((dmclient->cstate == DMSERVER_CLIENT_ESTABLISHING) || (dmclient->cstate == DMSERVER_CLIENT_ESTABLISHED)){close(temp_cfd); return;}
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "_dmserver_worker_main() - Client %d connection stage TCP ok.", temp_cfd);
    dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "_dmserver_worker_main() - Client %d assigned to point (%lu, %lu).", temp_cfd, temp_thindex, temp_cindex);

    // Set the connection data into the selected client slot & add fd to the subthread epoll:
    if(!_dmserver_cconn_set(dmclient, &(dmserver_cliloc_t){.th_pos=temp_thindex, .wc_pos=temp_cindex}, temp_cfd, &temp_caddr, NULL)) {
        close(temp_cfd); 
        return;
    }

    // Add the connected client to the subordinate thread:
    if (dmserver->sconn.sssl_enable) {
        // TCP + TLS(establishing):
        dmclient->cstate = DMSERVER_CLIENT_ESTABLISHING;

        // SSL object:
        dmclient->cssl = SSL_new(dmserver->sconn.sssl_ctx);
        if (!dmclient->cssl) {
            epoll_ctl(dmserver->sworker.wmainepfd, EPOLL_CTL_DEL, dmclient->cfd, NULL);
            close(dmclient->cfd);
            _dmserver_cconn_reset(dmclient);
            return;    
        }

        // Create BIO for the socket (non-blocking I/O):
        dmclient->cbio = BIO_new_socket(dmclient->cfd, BIO_NOCLOSE);
        if (!dmclient->cbio) {
            SSL_free(dmclient->cssl);
            epoll_ctl(dmserver->sworker.wmainepfd, EPOLL_CTL_DEL, dmclient->cfd, NULL);
            close(dmclient->cfd);
            _dmserver_cconn_reset(dmclient);
            return;
        }

        // Assign BIO to SSL object for both read and write operations:
        SSL_set_bio(dmclient->cssl, dmclient->cbio, dmclient->cbio);

        // Distribute the client to the subordinate thread:
        if (epoll_ctl(dmserver->sworker.wsubepfd[temp_thindex], EPOLL_CTL_ADD, dmclient->cfd, &(struct epoll_event){.events=EPOLLIN | EPOLLOUT | EPOLLET, .data.ptr=dmclient}) < 0){
            close(dmclient->cfd);
            _dmserver_cconn_reset(dmclient); 
            return;
        }

    } else {
        // TCP(established):
        dmclient->cstate = DMSERVER_CLIENT_ESTABLISHED;

        // Distribute the client to the subordinate thread:
        if (epoll_ctl(dmserver->sworker.wsubepfd[temp_thindex], EPOLL_CTL_ADD, dmclient->cfd, &(struct epoll_event){.events=EPOLLIN | EPOLLET, .data.ptr=dmclient}) < 0) {
            close(dmclient->cfd);
            _dmserver_cconn_reset(dmclient);
            return;
        }

        // Log message:
        char cip_str[INET6_ADDRSTRLEN];
        const void * addr = (dmclient->caddr_family == AF_INET) ? (void*)&dmclient->caddr.c4.sin_addr : (void*)&dmclient->caddr.c6.sin6_addr;
        inet_ntop(dmclient->caddr_family, addr, cip_str, sizeof(cip_str));
        int cport_num = (dmclient->caddr_family == AF_INET) ? ntohs(dmclient->caddr.c4.sin_port) : ntohs(dmclient->caddr.c6.sin6_port);
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Client %d with address %s:%d connected to server.\n", dmclient->cfd, cip_str, cport_num);
    }
    dmserver->sworker.wccount[temp_thindex]++;
}

/*
    @brief Helper function that implements the ssl handshake (in pseudo blocking mode - 20 attempts).

    @param SSL * temp_cssl: Reference to temporal client ssl object.
    @param int * temp_cfd: Reference to temporal client socket file descriptor.

    @retval false: SSL Handshake failed.
    @retval true: SSL Handshake succeeded.
*/
static bool _dmserver_helper_csslhandshake(dmserver_pt dmserver, dmserver_cliconn_pt c){
    // References check:
    if (!dmserver || !c) return false;
    if (!dmserver->sconn.sssl_enable || (c->cstate != DMSERVER_CLIENT_ESTABLISHING)) return true;

    // SSL Handshake process:
    int ssl_code = SSL_accept(c->cssl);
    int err = SSL_get_error(c->cssl, ssl_code);

    switch(err){
        case SSL_ERROR_NONE:
            // Hanshake completed successfuly:
            c->cstate = DMSERVER_CLIENT_ESTABLISHED;
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "_dmserver_worker_main() - Client %d connection stage TLS ok.", c->cfd);
            
            // Modification of events in client epoll fd:
            if (epoll_ctl(dmserver->sworker.wsubepfd[c->cloc.th_pos], EPOLL_CTL_MOD, c->cfd, &(struct epoll_event){.events=EPOLLIN|EPOLLET, .data.ptr=c}) < 0){
                // EPOLL error:
                SSL_shutdown(c->cssl);
                SSL_free(c->cssl);
                close(c->cfd);
                _dmserver_cconn_reset(c);
                return false;  
            }

            // Log message:
            char cip_str[INET6_ADDRSTRLEN];
            const void * addr = (c->caddr_family == AF_INET) ? (void*)&c->caddr.c4.sin_addr : (void*)&c->caddr.c6.sin6_addr;
            inet_ntop(c->caddr_family, addr, cip_str, sizeof(cip_str));
            int cport_num = (c->caddr_family == AF_INET) ? ntohs(c->caddr.c4.sin_port) : ntohs(c->caddr.c6.sin6_port);
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Client %d with address %s:%d connected to server.\n", c->cfd, cip_str, cport_num);
            return true;

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            // Handshake in progress:
            return false;

        case SSL_ERROR_ZERO_RETURN:
        case SSL_ERROR_SSL:
        case SSL_ERROR_SYSCALL:
        default:
            // Fatal/Unknown error detected, clean client and return:
            SSL_shutdown(c->cssl);
            SSL_free(c->cssl);
            close(c->cfd);
            _dmserver_cconn_reset(c);
            return false;
        }      
    return false;
}

/*
    @brief Helper function that implements a client timeout process.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param struct dmserver_cliconn * dmclient: Reference to the client to check timeout.
    @param size_t dmthindex: Caller thread index.

    @retval false: Client timedout and disconnected.
    @retval true: Client not timedout.
*/
static bool _dmserver_helper_cctimeout(dmserver_pt dmserver, dmserver_cliconn_pt dmclient){
    // Refernces & state check:
    if (!dmserver || !dmclient) return false;
    if ((dmclient->cstate != DMSERVER_CLIENT_ESTABLISHED) && (dmclient->cstate != DMSERVER_CLIENT_ESTABLISHING)) return false;

    // Timeout check and process:
    if(!_dmserver_cconn_checktimeout(dmclient, dmserver->sworker.wth_clistimeout)){
        if (dmserver->scallback.on_client_timeout) dmserver->scallback.on_client_timeout(dmclient);
        dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Client %d timedout, closing connection...", dmclient->cfd);
        dmserver_disconnect(dmserver, &(dmserver_cliloc_t){.th_pos=dmclient->cloc.th_pos, .wc_pos=dmclient->cloc.wc_pos});
        return false;
    }

    return true;
}

/*
    @brief Helper function that implements the read process.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param struct dmserver_cliconn * dmclient: Reference to the client to check timeout.
    @param size_t dmthindex: Caller thread index.
    @param struct epoll_events * evs: Reference to the array of events generated.
    @param size_t evindex: Event to process.

    @retval false: If read process lead to client disconnection.
    @retval true: If read process finished correctly.
*/
static bool _dmserver_helper_ccread(dmserver_pt dmserver, dmserver_cliconn_pt dmclient, size_t dmthindex, struct epoll_event * evs, size_t evindex){
    // References check:
    if (!dmserver || !dmclient || !evs) return false;

    // Read process:
    if (evs[evindex].events & EPOLLIN){
        // Read lock of client:
        pthread_mutex_lock(&dmclient->crlock);

        // Read bytes from clients (encrypted/decrypted optional) to client read buffer:
        int rb = 0;
        int rb_err = 0;
        if (dmserver->sconn.sssl_enable){
            rb = SSL_read(dmclient->cssl, dmclient->crbuffer, dmclient->crbuffer_size-1);
            rb_err = SSL_get_error(dmclient->cssl, rb);
        } else {
            rb = read(dmclient->cfd, dmclient->crbuffer, dmclient->crbuffer_size-1);
            rb_err = errno;
        }
        dmclient->crbuffer[dmclient->crbuffer_size - 1] = '\0';

        if (rb > 0){
            // Data reception case:
            dmclient->crlen += rb;
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "Read of %d bytes from client %d.\n", rb, dmclient->cfd);

            // Timeout ctl update:
            dmclient->clastt =  time(NULL);
            
            // User specific data processing of received data and read buffer reset afterwards:
            if (dmserver->scallback.on_client_rcv) dmserver->scallback.on_client_rcv(dmclient);
            memset(dmclient->crbuffer, 0, dmclient->crbuffer_size);

        } else if ((rb == 0) || ((rb_err == SSL_ERROR_ZERO_RETURN) && dmserver->sconn.sssl_enable)){
            // Client disconnect case:
            dmserver_disconnect(dmserver, &(dmserver_cliloc_t){.th_pos=dmclient->cloc.th_pos, .wc_pos=dmclient->cloc.wc_pos});
            pthread_mutex_unlock(&dmclient->crlock);
            return false;

        } else if ((((rb_err != SSL_ERROR_WANT_READ) && (rb_err != SSL_ERROR_WANT_WRITE)) && dmserver->sconn.sssl_enable) || (((rb_err != EAGAIN) && (rb_err != EWOULDBLOCK) && (rb_err != EINTR)) && !dmserver->sconn.sssl_enable)){
            // Comunication error case:
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Client %d com. failed, forced disconnection.", dmclient->cfd); 
            dmserver_disconnect(dmserver, &(dmserver_cliloc_t){.th_pos=dmclient->cloc.th_pos, .wc_pos=dmclient->cloc.wc_pos});
            pthread_mutex_unlock(&dmclient->crlock);
            return false;
        }

        // Read unlock of client:
        pthread_mutex_unlock(&dmclient->crlock);
    }

    return true;
}

/*
    @brief Helper function that implements the write process.

    @param dmserver_pt dmserver: Reference to dmserver struct.
    @param struct dmserver_cliconn * dmclient: Reference to the client to check timeout.
    @param size_t dmthindex: Caller thread index.
    @param struct epoll_events * evs: Reference to the array of events generated.
    @param size_t evindex: Event to process.

    @retval false: If write process lead to client disconnection.
    @retval true: If write process finished correctly.
*/
static bool _dmserver_helper_ccwrite(dmserver_pt dmserver, dmserver_cliconn_pt dmclient, size_t dmthindex, struct epoll_event * evs, size_t evindex){
    // References check:
    if (!dmserver || !dmclient || !evs) return false;
    
    // Write process:
    if ((evs[evindex].events & EPOLLOUT) && (dmclient->cwlen > 0)){
        // Write lock of client:
        pthread_mutex_lock(&dmclient->cwlock);

        // Write bytes from clients (encrypted/decrypted optional):
        int wb = 0;
        int wb_err = 0;
        if (dmserver->sconn.sssl_enable){
            wb = SSL_write(dmclient->cssl, dmclient->cwbuffer, dmclient->cwlen);
            wb_err = SSL_get_error(dmclient->cssl, wb);
        } else {
            wb = write(dmclient->cfd, dmclient->cwbuffer, dmclient->cwlen);
            wb_err = errno;
        }

        if (wb > 0){
            // Data sent case:
            dmclient->cwlen = wb;
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_DEBUG, "Write of %d bytes from client %d.\n", wb, dmclient->cfd);

            // Disable output events:
            if (epoll_ctl(dmserver->sworker.wsubepfd[dmthindex], EPOLL_CTL_MOD, dmclient->cfd, &(struct epoll_event){.events=EPOLLIN | EPOLLET, .data.ptr=dmclient}) < 0){
                // All data send error case:
                dmserver_disconnect(dmserver, &(dmserver_cliloc_t){.th_pos=dmclient->cloc.th_pos, .wc_pos=dmclient->cloc.wc_pos});
                pthread_mutex_unlock(&dmclient->cwlock); 
                return false;
            }

            // Write data user callback and reset:
            if (dmserver->scallback.on_client_snd) dmserver->scallback.on_client_snd(dmclient);
            memset(dmclient->cwbuffer, 0, dmclient->cwbuffer_size);
            dmclient->cwlen = 0;
             
        } else if ((((wb_err != SSL_ERROR_WANT_READ) && (wb_err != SSL_ERROR_WANT_WRITE)) && dmserver->sconn.sssl_enable) || (((wb_err != EAGAIN) && (wb_err != EWOULDBLOCK) && (wb_err != EINTR)) && !dmserver->sconn.sssl_enable)) {
            // Comunication error case:
            dmlogger_log(dmserver->slogger, DMLOGGER_LEVEL_INFO, "Client %d com. failed, forced disconnection.", dmclient->cfd); 
            dmserver_disconnect(dmserver, &(dmserver_cliloc_t){.th_pos=dmclient->cloc.th_pos, .wc_pos=dmclient->cloc.wc_pos});
            pthread_mutex_unlock(&dmclient->cwlock); 
            return false;
        }

        // Write unlock of clients:
        pthread_mutex_unlock(&dmclient->cwlock);
    }

    return true;
}