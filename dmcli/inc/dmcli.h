/*
    =========
      DMCLI
    =========

    Dmcli is a simple tool to create a command line interface in a simple and intuitive manner.

    The objective is to avoid redundancy in programs that aims to control a central unit or any other tool
    with commands in raw text on a terminal.

    --------
    Author: Antonio Carretero Sahuquillo
    Date: 2025-09-26

    Licensed under the GNU General Public License v3.0 (GPLv3).
    See https://www.gnu.org/licenses/gpl-3.0.html for details.
*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMCLI_HEADER
#define _DMCLI_HEADER

/* ---- Libraries ------------------------------------------------- */
#include "dmcli_io.h"


/* ---- Defines & macros ------------------------------------------ */
/* ---- Enumerations ---------------------------------------------- */
/* ---- Data structures ------------------------------------------- */
// Simple dmcli command system:
struct dmcli_cmd{
    char ** cmd;
    void (**fn) (void * udata); 
};

// General data structure of dmcli:
struct dmcli{
    struct dmcli * supcli;
    struct dmcli * subcli;
    dmcli_io_pt io;
    struct dmcli_cmd cmd;
};

/* ---- Data types ------------------------------------------------ */
// General data type of dmcli:
typedef struct dmcli dmcli_t;
typedef dmcli_t * dmcli_pt;

/* ---- Functions prototypes -------------------------------------- */
// Initialization / Loop / Deinitialization:
void dmcli_init(dmcli_pt * dmcli);
void dmcli_loop(dmcli_pt dmcli);
void dmcli_deinit(dmcli_pt * dmcli);

#endif