/*
    ============
      DMLOGGER
    ============

    Dmlogger is a simple yet well-balanced logging utility designed for Unix systems. 
    Architecture type is MPSC (Multiple producers single consumer) 
    
    It provides non-blocking logging capabilities, meaning that log operations are intended to run "in
    parallel" while the caller thread continues its execution independently.
    
    This library offers basic functions to keep it simple to use, easy to read, and straightforward
    to integrate into separate projects.

    --------
    Author: Antonio Carretero Sahuquillo
    Date: 2025-08-22

    Licensed under the GNU General Public License v3.0 (GPLv3).
    See https://www.gnu.org/licenses/gpl-3.0.html for details.
*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMLOGGER_HEADER
#define _DMLOGGER_HEADER

/* ---- Libraries ------------------------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include <pthread.h>
#include <time.h>
#include <string.h>
#include <errno.h>

/* ---- Defines & macros ------------------------------------------ */
#define DEFAULT_ENTRY_TIMESTAMPLEN 32
#define DEFAULT_ENTRY_LEVELLEN 8
#define DEFAULT_ENTRY_MESSAGELEN 128
#define DEFAULT_ENTRY_EXTRALEN 32

#define DEFAULT_OUTPUT_FILE_PATH "./log/"    
#define DEFAULT_OUTPUT_FILE_BASENAME "log"
#define DEFAULT_OUTPUT_FILE_PATHLEN 1024
#define DEFAULT_OUTPUT_FILE_BASENAMELEN 128
#define DEFAULT_OUTPUT_FILE_EXTRALEN 32
#define DEFAULT_OUTPUT_FILE_ROTATE_MAXSIZE 10000000 // 10MB
#define DEFAULT_OUTPUT_OSEL 1   // stdout


#define DEFAULT_QUEUE_CAPACITY 200      // 200 entries by default.
#define DEFAULT_QUEUE_OVERFLOW_POLICY 0 // Drop new logs when queue overflow.
#define DEFAULT_QUEUE_WAIT_TIMEOUT 1    // One second timeout when policy is stablish to wait with timeout.
#define DEFAULT_LOG_MINLVL 1            // INFO by default.

#define DMLOGGER_LEVEL2STRING(level)    ((level) == 1 ? "DEBUG" :\
                                        ((level) == 2 ? "INFO" :\
                                        ((level) == 3 ? "NOTIFY" :\
                                        ((level) == 4 ? "WARNING" :\
                                        ((level) == 5 ? "ERROR" :\
                                        ((level) == 6 ? "FATAL" :\
                                        "-"))))))
/* ---- Enumerations ---------------------------------------------- */
// States of dmlogger (in dmlogger):
enum dmlogger_state{
    DMLOGGER_STATE_RUNNING,
    DMLOGGER_STATE_STOPPING,
    DMLOGGER_STATE_STOPPED,
};

// Levels of dmlogger (in dmlogger):
enum dmlogger_level{
    DMLOGGER_LEVEL_DEBUG = 1,
    DMLOGGER_LEVEL_INFO,
    DMLOGGER_LEVEL_NOTIFY,
    DMLOGGER_LEVEL_WARNING,
    DMLOGGER_LEVEL_ERROR,
    DMLOGGER_LEVEL_FATAL
};

// Queue overflow policies (in dmlogger_queue)
enum dmlogger_queue_ofpolicy{
    DMLOGGER_OFPOLICY_DROP,
    DMLOGGER_OFPOLICY_OVERWRITE,
    DMLOGGER_OFPOLICY_WAIT,
    DMLOGGER_OFPOLICY_WAIT_TIMEOUT 
};

// Output selection (in dmlogger_output)
enum dmlogger_output_sel{
    DMLOGGER_OUTPUT_FILE,
    DMLOGGER_OUTPUT_STDOUT,
    DMLOGGER_OUTPUT_STDERR,
    DMLOGGER_OUTPUT_CUSTOM
};

/* ---- Data structures ------------------------------------------- */
// Output data structure for dmlogger:
struct dmlogger_output{       
    enum dmlogger_output_sel osel;          // Selección de método de salida de log.
    pthread_mutex_t mutex;                     // File output mutex.
    union{
        struct {
            char path[DEFAULT_OUTPUT_FILE_PATHLEN];           // File path name to log output.         
            char basename[DEFAULT_OUTPUT_FILE_BASENAMELEN];   // File basename of log output.
            time_t date;                                      // File creation date.
            bool date_rot;                                    // File date rotation flag.
            size_t size;                                      // File size in real time.
            bool size_rot;                                    // File size rotation flag.
            size_t max_size;                                  // File maximum size in Bytes.
            size_t index;                                     // File index.
            FILE * fd;                                        // File descriptor.
        }file;

        struct {
            FILE * stream;                   // Standard output stream.  
        }stdio;

        struct {
            void (*cwrite_fn)(const char * lmsg, void * cdata);  // Custom function to write output.
            void * cdata;                                       // Custom data pointer for output.
        }custom;
    };                         
};

// Entry data structure for dmlogger:
struct dmlogger_entry{
    char timestamp[DEFAULT_ENTRY_TIMESTAMPLEN];                     // Date string to be logged.
    char level[DEFAULT_ENTRY_LEVELLEN];                             // Level string to be logged.
    char message[DEFAULT_ENTRY_MESSAGELEN];                         // Message string to be logged.
};

// Entry (circular) queue data structure for dmlogger:
struct dmlogger_queue{
    enum dmlogger_queue_ofpolicy of_policy; // Entry queue overflow policy.
    unsigned int wait_timeout;              // Entry queue wait policy timeout in seconds.
    struct dmlogger_entry * equeue;         // Entry queue.
    pthread_mutex_t prod_mutex;             // Entry queue producer mutex.
    pthread_cond_t prod_cond;               // Entry queue producer cond.
    pthread_mutex_t cons_mutex;             // Entry queue consumer mutex (needed for cons_cond).
    pthread_cond_t cons_cond;               // Entry queue consumer cond.
    size_t capacity;                        // Entry queue capacity (max number of entries in queue).
    size_t head;                            // Entry queue head slot.
    size_t tail;                            // Entry queue tail slot.
};

// General data structure dmlogger:
struct dmlogger{
    struct dmlogger_output output;          // Output structure of dmlogger.
    struct dmlogger_queue queue;            // Queue structure of dmlogger.
    pthread_t logger_th;                    // Logger (consumer) thread - Only one per instance.
    enum dmlogger_level min_level;          // Minimum level to be logged of dmlogger.
    enum dmlogger_state state;              // State of dmlogger.
};

/* ---- Data types ------------------------------------------------ */
// Data type and pointer data type for dmlogger:
typedef struct dmlogger dmlogger_t;
typedef dmlogger_t * dmlogger_pt;

/* ---- Functions prototypes -------------------------------------- */
// Initialization / Run / Deinitialization:
void dmlogger_init(dmlogger_pt * dmlogger);
bool dmlogger_run(dmlogger_pt dmlogger);
void dmlogger_deinit(dmlogger_pt * dmlogger);

// Configuration - Output:
bool dmlogger_conf_output_file(dmlogger_pt dmlogger, const char * file_path, const char * file_basename, bool file_rotate_bydate, bool file_rotate_bysize, size_t file_rotate_maxsize);
bool dmlogger_conf_output_stdout(dmlogger_pt dmlogger);
bool dmlogger_conf_output_stderr(dmlogger_pt dmlogger);
bool dmlogger_conf_output_custom(dmlogger_pt dmlogger, void (*cwrite_fn)(const char *, void *), void * cdata);

// Configuration - Queue:
bool dmlogger_conf_queue_ofpolicy(dmlogger_pt dmlogger, enum dmlogger_queue_ofpolicy queue_ofpolicy, unsigned int wait_timeout);
bool dmlogger_conf_queue_capacity(dmlogger_pt dmlogger, size_t queue_capacity);

// Configuration - Level:
bool dmlogger_conf_logger_minlvl(dmlogger_pt dmlogger, enum dmlogger_level min_level);

// Log (pritnf-style formatting):
void dmlogger_log(dmlogger_pt dmlogger, enum dmlogger_level level, const char * log_fmsg, ...);

#endif