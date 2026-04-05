#ifndef _DM_LINKEDLISTS_HEADER
#define _DM_LINKEDLISTS_HEADER

/*---- Bibliotecas. ----*/
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/*---- Constantes. ----*/

/*---- Enumeraciones. ----*/
// Tipos de listas enlazadas:
typedef enum dmll_node_type{
    DMSLL_NODE,
    DMDLL_NODE,
    DMCSLL_NODE,
    DMCDLL_NODE
}dmll_node_t;

// Tipos de errores:
typedef enum dmll_error_type{
    DMLL_OK,
    DMLL_ERR,

    DMLL_INVALID_REFERENCE,
    DMLL_INVALID_NODE_TYPE,
    DMLL_NO_MEMORY,
    DMLL_EMPTY,

    DMLL_NODE_NOT_FOUND,
}dmll_error_t;

/*---- Tipos de datos. -----*/
typedef struct dmll dmll_t;
typedef struct dmll_conf dmll_conf_t;

/*---- Estructuras de datos. ----*/
// Lista enlazada simple:
struct dmll_node{
    void * data;
    struct dmll_node * next;
    struct dmll_node * prev;
};

// Estructura general de funcionalidad:
struct dmll_fn{
    dmll_error_t (* dmll_insert) (dmll_t * const, void * const);
    dmll_error_t (* dmll_delete) (dmll_t * const, void * const, bool (*) (void *, void *));

    dmll_error_t (* dmll_foreach) (dmll_t * const, void (*)(void *));
    struct dmll_node * (* dmll_find) (dmll_t * const, void * const, bool (*)(void *, void *));
};

// Estructura general de listas enlazadas:
struct dmll{
    dmll_error_t error_type;
    dmll_node_t node_type;
    size_t ll_len;
    struct dmll_node *ll_head;
    struct dmll_node *ll_tail;
    struct dmll_fn fn;
    void (* free_fn) (void *);
};

// Estructura de configuración de la estructura general:
struct dmll_conf{
    dmll_node_t node_type;
    void (* free_fn) (void *);
};

/*---- Prototipos de funciones. ----*/
dmll_error_t dmll_init(dmll_t ** const dmll, const dmll_conf_t * const dmll_conf);
dmll_error_t dmll_deinit(dmll_t ** const dmll);

#endif