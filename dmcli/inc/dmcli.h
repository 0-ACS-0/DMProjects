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
#include "dmcli_hdrs.h"
#include "dmcli_io.h"
#include "dmcli_cmd.h"

/* ---- Defines & macros ------------------------------------------ */
/* ---- Enumerations ---------------------------------------------- */
/* ---- Data structures ------------------------------------------- */
// General data structure of dmcli:
struct dmcli{
    dmcli_io_t io;
    dmcli_cmd_t cmd;
    bool is_running;
};

/* ---- Data types ------------------------------------------------ */
// General data type of dmcli:
typedef struct dmcli dmcli_t;
typedef dmcli_t * dmcli_pt;

/* ---- Functions prototypes -------------------------------------- */
// Initialization / CLI Loop:
bool dmcli_init(dmcli_pt dmcli);
void dmcli_loop(dmcli_pt dmcli);

#endif