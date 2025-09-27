/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMCLI_IO_HEADER
#define _DMCLI_IO_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "dmcli_hdrs.h"


/* ---- Defines & macros ------------------------------------------ */
#define DEFAULT_IO_PROMPT_STRING ">> "
#define DEFAULT_IO_ILOG_CAP 20
#define DEFAULT_IO_INPUT_CAP 10000

/* ---- Data structures ------------------------------------------- */
// Simple dmcli conf structure (used externally for customized configuration functions only):
struct dmcli_io_conf{
};

// Simple dmcli io structure:
struct dmcli_io{
    struct termios orig_termios;    // Original terminal input/output stream configuration.
    bool is_raw;                    // Flag to indicate the current state of the terminal (raw or not raw). 

    char * prompt;          // Prompt string (shows the user when to input data).
    size_t prompt_length;   // Pormpt string length in characters.

    char * input;           // User input, stores the most recent user input data.
    size_t input_capacity;  // User input capacity (characters) available.
    size_t input_length;    // User input lenght in characters.  
    size_t input_cursor;    // User input cursor position.

    char ** ilog;           // Input log, stores a historical of the user input.
    size_t ilog_length;     // The length of the ilog member.
    size_t ilog_capacity;   // The maximum capacity of the ilog member. 
};

/* ---- Data types ------------------------------------------------ */
// Configuration type:
typedef struct dmcli_io_conf dmcli_io_conf_t;
typedef dmcli_io_conf_t * dmcli_io_conf_pt;

// Simple dmcli io data type:
typedef struct dmcli_io dmcli_io_t;
typedef dmcli_io_t * dmcli_io_pt;

/* ---- Functions prototypes -------------------------------------- */
// Alloc / Dealloc:
void dmcli_io_alloc(dmcli_io_pt * dmcli_io);
void dmcli_io_dealloc(dmcli_io_pt * dmcli_io);

// "Setters" / "Getters":
bool dmcli_io_set_default(dmcli_io_pt dmcli_io);
bool dmcli_io_set_prompt(dmcli_io_pt dmcli_io, const char * prompt_str);
bool dmcli_io_set_inputcap(dmcli_io_pt dmcli_io, size_t input_capacity);
bool dmcli_io_set_ilogcap(dmcli_io_pt dmcli_io, size_t ilog_capacity);

char * dmcli_io_get_prompt(dmcli_io_pt dmcli_io);
size_t dmcli_io_get_inputcap(dmcli_io_pt dmcli_io);
size_t dmcli_io_get_inputlen(dmcli_io_pt dmcli_io);
char * dmcli_io_get_input(dmcli_io_pt dmcli_io);
size_t dmcli_io_get_ilogcap(dmcli_io_pt dmcli_io);
size_t dmcli_io_get_iloglen(dmcli_io_pt dmcli_io);
char * dmcli_io_get_ilogat(dmcli_io_pt dmcli_io, size_t ilog_index);

// Terminal raw mode ctl:
bool dmcli_io_enterm_rawmode(dmcli_io_pt dmcli_io);
bool dmcli_io_disterm_rawmode(dmcli_io_pt dmcli_io);

// Utils:
bool dmcli_io_wait4input(dmcli_io_pt dmcli_io);

#endif