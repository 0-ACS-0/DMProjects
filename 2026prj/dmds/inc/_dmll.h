#ifndef _DM_LINKEDLISTS_HEADER
#define _DM_LINKEDLISTS_HEADER

/*---- Bibliotecas. ----*/
#include <stdlib.h>

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

    DMLL_InvalidReference,
    DMLL_NoMemory
}dmll_error_t;

/*---- Estructuras de datos. ----*/
// Lista enlazada simple:
struct dmsll_node{
    void * data;
    struct dmsll_node * next;
};

// Lista doblemente enlazada 
struct dmdll_node{
    void * data;
    struct dmdll_node * next;
    struct dmdll_node * prev;
};

// Lista circular enlazada simple: 
struct dmcsll_node{
    void * data;
    struct dmcsll_node * next;
};

// Lista circular doblemente enlazada:
struct dmcdll_node{
    void * data;
    struct dmcdll_node * next;
    struct dmcdll_node * prev;
};

// Estructura general de funcionalidad:
struct dmll_fn{
};

// Estructura general de listas enlazadas:
struct dmll{
    dmll_error_t error_type;
    dmll_node_t node_type;
    union{
        struct dmsll_node *sll;
        struct dmdll_node *dll;
        struct dmcsll_node * csll;
        struct dmcdll_node * cdll;
    }linked_list;
    struct dmll_fn fn;
};

// Estructura de configuración de la estructura general:
struct dmll_conf{
    dmll_node_t node_type;
};

/*---- Tipos de datos. -----*/
typedef struct dmll dmll_t;
typedef struct dmll_conf dmll_conf_t;

/*---- Prototipos de funciones. ----*/
dmll_error_t dmll_init(dmll_t ** dmll, const dmll_conf_t * dmll_conf);


#endif