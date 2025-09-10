/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMSERVER_WORKER_HEADER
#define _DMSERVER_WORKER_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "dmserver_hdrs.h"
#include "dmserver_cliconn.h"

/* ---- Defines  -------------------------------------------------- */
#define DEFAULT_WORKER_SUBTHREADS 8
#define DEFAULT_WORKER_CLISPERSTH 200
#define DEFAULT_WORKER_CLITIMEOUT 120

/* ---- Data structures ------------------------------------------- */
// Worker suthreads argument struct:
struct dmserver_subthreads_args{
    void * dmserver;
    size_t subthindex;
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
};

/* ---- Data types ------------------------------------------------ */
typedef struct dmserver_subthreads_args dmserver_subthargs_t;

/* ---- INTERNAL - Static functions prototypes -------------------- */
void * _dmserver_worker_main(void * args);
void * _dmserver_worker_sub(void * args);
void * _dmserver_subworker_timeout(void * args);


#endif