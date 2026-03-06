/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMSERVER_WORKER_HEADER
#define _DMSERVER_WORKER_HEADER
 
/* ---- Libraries ------------------------------------------------- */
#include "_dmserver_hdrs.h"
#include "_dmserver_cliconn.h"

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

    size_t wth_subthreads;
    pthread_t * wsubth;
    int * wsubepfd;

    // Clients placeholder for each sub-thread:
    size_t wth_clispersth;
    struct dmserver_cliconn ** wcclis;
    size_t * wccount;

    size_t wth_clistimeout;
    time_t wctimeout;
};

// Worker configuration data structure:
struct dmserver_worker_conf{
    size_t wth_subthreads;
    size_t wth_clispersth;
    size_t wth_clistimeout;
};

/* ---- Data types ------------------------------------------------ */
// Worker type:
typedef struct dmserver_worker dmserver_worker_t;
typedef dmserver_worker_t * dmserver_worker_pt;

// Worker configuration type:
typedef struct dmserver_worker_conf dmserver_worker_conf_t;
typedef dmserver_worker_conf_t * dmserver_worker_conf_pt;

// Subordinate threads argument data structure type:
typedef struct dmserver_subthreads_args dmserver_subthargs_t;

/* ---- INTERNAL - Static functions prototypes -------------------- */
// Worker threads:
void * _dmserver_worker_main(void * args);
void * _dmserver_worker_sub(void * args);
void * _dmserver_subworker_timeout(void * args);

// Worker allocators:
bool __dmserver_worker_alloc(dmserver_worker_pt w);
bool __dmserver_worker_dealloc(dmserver_worker_pt w);

// Worker configurations:
void __dmserver_worker_set_defaults(dmserver_worker_pt w);
void __dmserver_worker_set_subthreads(dmserver_worker_pt w, size_t wth_subthreads);
void __dmserver_worker_set_clispersth(dmserver_worker_pt w, size_t wth_clispersth);
void __dmserver_worker_set_clistimeout(dmserver_worker_pt w, size_t wth_clistimeout);

#endif