/*

*/

/* ---- Header guard ---------------------------------------------- */
#ifndef _DMSERVER_HDRS_HEADER
#define _DMSERVER_HDRS_HEADER
#ifndef _GNU_SOURCE 
#define _GNU_SOURCE
#endif

/* ---- Libraries ------------------------------------------------- */
// Standard:
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Unix:
#include <unistd.h>

// Character strings manipulation:
#include <string.h>

// Error codes to identify error conditions:
#include <errno.h>

// Threads:
#include <pthread.h>

// Network:
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 

// Events I/O:
#include <sys/epoll.h>

// OpenSSL (TLS):
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

// Signals:
#include <signal.h>

// Time for timeout:
#include <time.h>

// Logging system (from my dm projects :D)
#include "../libs/dmlogger.h"


#endif