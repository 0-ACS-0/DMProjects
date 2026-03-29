/*---- Cabecera ----*/
#include "../inc/_dmll.h"

/*---- Implementación ----*/
/*
    @brief Función para inicializar una lista enlazada.

    @param dmll_t ** dmll: Referencia a puntero a la estructura general de la lista enlazada.
    @param const dmll_conf_t * dmll_conf: Referencia (protegida) a la estructura de configuración.

    @retval dmll_error_t.
*/
dmll_error_t dmll_init(dmll_t ** dmll, const dmll_conf_t * dmll_conf){
    // Comprobación de referencias:
    if (!dmll || !dmll_conf) return DMLL_InvalidReference;

    // Reserva de memoria de la estructura general:
    *dmll = calloc(1, sizeof(dmll_t));
    if (!*dmll) return DMLL_NoMemory;

    // Configuración de la estructura general:
    (*dmll)->error_type = DMLL_OK;
    (*dmll)->node_type = dmll_conf->node_type;

    // Asignación de funcionalidad según tipo de nodo:
    //(*dmll)->fn.

    return DMLL_OK;
}

/*
    @brief Función pasa desinicializar una lista enlazada.

    @param dmll_t ** dmll: Referencia a puntero a la estructura general de la lista enlazada.

    @retval dmll_error_t.
*/
dmll_error_t dmll_deinit(dmll_t ** dmll){
    // Comprobación de referencia:
    if (!dmll || !*dmll) return DMLL_InvalidReference;

    // Liberación de memorioa:
    // TODO: cuando se implemente la lista (y su memoria) debe ser llamada una función para vaciarla aquí.
    free(*dmll);
    *dmll = NULL;

    return DMLL_OK;
}