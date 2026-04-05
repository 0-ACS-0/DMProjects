/*---- Cabecera ----*/
#include "../inc/_dmll.h"

/*---- Prototipos de las funciones privadas ----*/

// Funciones de inserción:
static dmll_error_t dmll_insert_sll(dmll_t * const dmll, void * const data);
static dmll_error_t dmll_insert_dll(dmll_t * const dmll, void * const data);
static dmll_error_t dmll_insert_csll(dmll_t * const dmll, void * const data);
static dmll_error_t dmll_insert_cdll(dmll_t * const dmll, void * const data);

// Funciones de eliminación:
static dmll_error_t dmll_delete_sll(dmll_t * const dmll, void * const data, bool (* cmp_fn) (void *, void *));
static dmll_error_t dmll_delete_dll(dmll_t * const dmll, void * const data, bool (* cmp_fn) (void *, void *));
static dmll_error_t dmll_delete_csll(dmll_t * const dmll, void * const data, bool (* cmp_fn) (void *, void *));
static dmll_error_t dmll_delete_cdll(dmll_t * const dmll, void * const data, bool (* cmp_fn) (void *, void *));

// Función de búsqueda:
static struct dmll_node * dmll_find(dmll_t * const dmll, void * const data, bool (* cmp_fn)(void *, void *));

// Función de aplicación:
static dmll_error_t dmll_foreach(dmll_t * const dmll, void (* fn)(void *));


/*---- Implementación ----*/
/*
    @brief Función para inicializar una lista enlazada.

    @param dmll_t ** const dmll: Referencia a puntero a la estructura general de la lista enlazada.
    @param const dmll_conf_t * const dmll_conf: Referencia (protegida) a la estructura de configuración.
    @retval dmll_error_t.
*/
dmll_error_t dmll_init(dmll_t ** const dmll, const dmll_conf_t * const dmll_conf){
    // Comprobación de referencias:
    if (!dmll || !dmll_conf) return DMLL_INVALID_REFERENCE;

    // Reserva de memoria de la estructura general:
    *dmll = calloc(1, sizeof(dmll_t));
    if (!*dmll) return DMLL_NO_MEMORY;

    // Configuración de la estructura general:
    (*dmll)->error_type = DMLL_OK;
    (*dmll)->node_type = dmll_conf->node_type;
    (*dmll)->free_fn = dmll_conf->free_fn;

    // Asignación de funcionalidad común:
    (*dmll)->fn.dmll_find = dmll_find;
    (*dmll)->fn.dmll_foreach = dmll_foreach;

    // Asignación de funcionalidad según tipo de nodo:
    switch ((*dmll)->node_type) {
        case DMSLL_NODE:
            (*dmll)->fn.dmll_insert = dmll_insert_sll;
            (*dmll)->fn.dmll_delete = dmll_delete_sll;
            break;
        case DMDLL_NODE:
            (*dmll)->fn.dmll_insert = dmll_insert_dll;
            (*dmll)->fn.dmll_delete = dmll_delete_dll;
            break;
        case DMCSLL_NODE:
            (*dmll)->fn.dmll_insert = dmll_insert_csll;
            (*dmll)->fn.dmll_delete = dmll_delete_csll;
            break;
        case DMCDLL_NODE:
            (*dmll)->fn.dmll_insert = dmll_insert_cdll;
            (*dmll)->fn.dmll_delete = dmll_delete_cdll;
            break;
        default:
            free(*dmll);
            return DMLL_INVALID_NODE_TYPE;
    }

    return DMLL_OK;
}

/*
    @brief Función pasa desinicializar una lista enlazada.

    @param dmll_t ** const dmll: Referencia a puntero a la estructura general de la lista enlazada.
    @retval dmll_error_t.
*/
dmll_error_t dmll_deinit(dmll_t ** const dmll){
    // Comprobación de referencia:
    if (!dmll || !*dmll) return DMLL_INVALID_REFERENCE;

    // Ruptura de circularidad (si la hay y existe el nodo de cola):
    if ((*dmll)->ll_head) (*dmll)->ll_head->prev = NULL;
    if ((*dmll)->ll_tail) (*dmll)->ll_tail->next = NULL;

    // Liberación de memoria completa nodo a nodo:
    struct dmll_node * current_node = (*dmll)->ll_head;

    while ((current_node != NULL) && ((*dmll)->ll_len > 0)){
        struct dmll_node * next_node = current_node->next;
        if ((*dmll)->free_fn) (*dmll)->free_fn(current_node->data);
        free(current_node);
        current_node = next_node;
        (*dmll)->ll_len--;
    }

    // Liberación de memoria de la estructura general:
    free(*dmll);
    *dmll = NULL;

    return DMLL_OK;
}


/*---- Implementación - Privada ----*/
/*
    @brief Función para insertar un nodo a la lista.
    @note: SLL.

    @param dmll_t * dmll: Referencia a la lista enlazada.
    @param void * data: Referencia de dato genérico.
    @retval dmll_error_t.
*/
static dmll_error_t dmll_insert_sll(dmll_t * const dmll, void * const data){
    // Comprobación de referencia:
    if (!dmll) return DMLL_INVALID_REFERENCE;

    // Creación de nodo:
    struct dmll_node * i_node = calloc(1, sizeof(struct dmll_node));
    if (!i_node) return DMLL_NO_MEMORY;
    i_node->data = data;
    i_node->next = NULL;
    i_node->prev = NULL;

    // Caso de lista vacía:
    if (dmll->ll_head == NULL){
        dmll->ll_head = i_node;
        dmll->ll_tail = i_node;
    // Caso de lista con nodos:
    } else {
        dmll->ll_tail->next = i_node;
        dmll->ll_tail = i_node;
    }

    dmll->ll_len++;
    return DMLL_OK;
}

/*
    @brief Función para insertar un nodo a la lista.
    @note: DLL.

    @param dmll_t * dmll: Referencia a la lista enlazada.
    @param void * data: Referencia de dato genérico.
    @retval dmll_error_t.
*/
static dmll_error_t dmll_insert_dll(dmll_t * dmll, void * const data){
    // Comprobación de referencia:
    if (!dmll) return DMLL_INVALID_REFERENCE;

    // Creación de nodo:
    struct dmll_node * i_node = calloc(1, sizeof(struct dmll_node));
    if (!i_node) return DMLL_NO_MEMORY;
    i_node->data = data;
    i_node->next = NULL;
    i_node->prev = NULL;

    // Caso de lista vacía:
    if (dmll->ll_head == NULL){
        dmll->ll_head = i_node;
        dmll->ll_tail = i_node;
    // Caso de lista con nodos:
    } else {
        i_node->prev = dmll->ll_tail;
        dmll->ll_tail->next = i_node;
        dmll->ll_tail = i_node;
    }

    dmll->ll_len++;
    return DMLL_OK;
}

/*
    @brief Función para insertar un nodo a la lista.
    @note: CSLL.

    @param dmll_t * dmll: Referencia a la lista enlazada.
    @param void * data: Referencia de dato genérico.
    @retval dmll_error_t.
*/
static dmll_error_t dmll_insert_csll(dmll_t * dmll, void * const data){
    // Comprobación de referencia:
    if (!dmll) return DMLL_INVALID_REFERENCE;

    // Creación de nodo:
    struct dmll_node * i_node = calloc(1, sizeof(struct dmll_node));
    if (!i_node) return DMLL_NO_MEMORY;
    i_node->data = data;
    i_node->next = NULL;
    i_node->prev = NULL;

    // Caso de lista vacía:
    if (dmll->ll_head == NULL){
        dmll->ll_head = i_node;
        dmll->ll_tail = i_node;
        i_node->next = i_node;
        i_node->prev = i_node;
    // Caso de lista con nodos:
    } else {
        i_node->next = dmll->ll_head;
        dmll->ll_tail->next = i_node;
        dmll->ll_tail = i_node;
    }

    dmll->ll_len++;
    return DMLL_OK;
}

/*
    @brief Función para insertar un nodo a la lista.
    @note: CDLL.

    @param dmll_t * dmll: Referencia a la lista enlazada.
    @param void * data: Referencia de dato genérico.
    @retval dmll_error_t.
*/
static dmll_error_t dmll_insert_cdll(dmll_t * dmll, void * const data){
    // Comprobación de referencia:
    if (!dmll) return DMLL_INVALID_REFERENCE;

    // Creación de nodo:
    struct dmll_node * i_node = calloc(1, sizeof(struct dmll_node));
    if (!i_node) return DMLL_NO_MEMORY;
    i_node->data = data;
    i_node->next = NULL;
    i_node->prev = NULL;

    // Caso de lista vacía:
    if (dmll->ll_head == NULL){
        dmll->ll_head = i_node;
        dmll->ll_tail = i_node;
        i_node->next = i_node;
        i_node->prev = i_node;
    // Caso de lista con nodos:
    } else {
        i_node->next = dmll->ll_head;
        i_node->prev = dmll->ll_tail;
        dmll->ll_tail->next = i_node;
        dmll->ll_head->prev = i_node;
        dmll->ll_tail = i_node;
    }

    dmll->ll_len++;
    return DMLL_OK;
}

/*
    @brief Función para eliminar un nodo, con función personalizada.
    @note: SLL.

    @param dmll_t * dmll: Referencia a la estructura general de la lista enlazada.
    @param void * data: Datos del nodo a eliminar.
    @param bool (* cmp) (void *, void*): Función de comparación genérica.
    @retval dmll_error_t.
*/
static dmll_error_t dmll_delete_sll(dmll_t * const dmll, void * const data, bool (* cmp_fn) (void *, void *)){
    // Comprobación de referencias:
    if (!dmll || !cmp_fn) return DMLL_INVALID_REFERENCE;
    if (!dmll->ll_head) return DMLL_EMPTY;

    // Nodo actual vs previo para comprobación:
    struct dmll_node * current_node = dmll->ll_head;
    struct dmll_node * previous_node = NULL;

    // Comparación nodo a nodo:
    while (current_node != NULL) {
        // Nodo no encontrado, se continúa con el bucle:
        if (!cmp_fn(current_node->data, data)){
            previous_node = current_node;
            current_node = current_node->next;
            continue;
        }

        // Nodo encontrado, se actualizan punteros:
        if (previous_node == NULL) {                            // Nodo cabecera.
            dmll->ll_head = current_node->next;
            if (dmll->ll_head == NULL) dmll->ll_tail = NULL;
        } else {                                                // Nodo intermedio/cola.
            previous_node->next = current_node->next;
            if (current_node == dmll->ll_tail) dmll->ll_tail = previous_node;
        }

        // Liberación de memoria y eliminación del nodo:
        if (dmll->free_fn) dmll->free_fn(current_node->data);
        free(current_node);
        dmll->ll_len--;
        return DMLL_OK;
    }

    return DMLL_NODE_NOT_FOUND;
}

/*
    @brief Función para eliminar un nodo, con función personalizada.
    @note: DLL.

    @param dmll_t * dmll: Referencia a la estructura general de la lista enlazada.
    @param void * data: Datos del nodo a eliminar.
    @param bool (* cmp) (void *, void*): Función de comparación genérica.
    @retval dmll_error_t.
*/
static dmll_error_t dmll_delete_dll(dmll_t * const dmll, void * const data, bool (* cmp_fn) (void *, void *)){
    // Comprobación de referencias:
    if (!dmll || !cmp_fn) return DMLL_INVALID_REFERENCE;
    if (!dmll->ll_head) return DMLL_EMPTY;

    // Nodo actual vs previo para comprobación:
    struct dmll_node * current_node = dmll->ll_head;

    // Comparación nodo a nodo:
    while (current_node != NULL) {
        // Nodo no encontrado, se continúa con el bucle:
        if (!cmp_fn(current_node->data, data)){
            current_node = current_node->next;
            continue;
        }

        // Nodo encontrado, se actualizan punteros:
        if (current_node->prev == NULL) {               // Nodo cabecera.
            dmll->ll_head = current_node->next;
            if (dmll->ll_head == NULL) dmll->ll_tail = NULL; else dmll->ll_head->prev = NULL;
        } else if (current_node->next == NULL){         // Nodo cola.
            dmll->ll_tail = current_node->prev;
            dmll->ll_tail->next = NULL;
        } else {                                        // Nodo intermedio.
            current_node->prev->next = current_node->next;                                
            current_node->next->prev = current_node->prev;
            if (current_node == dmll->ll_tail) dmll->ll_tail = current_node->prev;
        }

        // Liberación de memoria y eliminación del nodo:
        if (dmll->free_fn) dmll->free_fn(current_node->data);
        free(current_node);
        dmll->ll_len--;
        return DMLL_OK;
    }

    return DMLL_NODE_NOT_FOUND;
}

/*
    @brief Función para eliminar un nodo, con función personalizada.
    @note: CSLL.

    @param dmll_t * dmll: Referencia a la estructura general de la lista enlazada.
    @param void * data: Datos del nodo a eliminar.
    @param bool (* cmp) (void *, void*): Función de comparación genérica.
    @retval dmll_error_t.
*/
static dmll_error_t dmll_delete_csll(dmll_t * const dmll, void * const data, bool (* cmp_fn) (void *, void *)){
    // Comprobación de referencias:
    if (!dmll || !cmp_fn) return DMLL_INVALID_REFERENCE;
    if (!dmll->ll_head) return DMLL_EMPTY;

    // Nodo actual vs previo para comprobación:
    struct dmll_node * initial_node = dmll->ll_head;
    struct dmll_node * current_node = dmll->ll_head;
    struct dmll_node * previous_node = dmll->ll_tail;

    // Comparación nodo a nodo:
    do {
        // Nodo no encontrado, se continúa con el bucle:
        if (!cmp_fn(current_node->data, data)){
            previous_node = current_node;
            current_node = current_node->next;
            continue;
        }

        // Nodo encontrado, se actualizan punteros:
        if ((current_node == dmll->ll_head) && (current_node == dmll->ll_tail)){    // Nodo único.
            dmll->ll_head = NULL;
            dmll->ll_tail = NULL;
        } else if (current_node == dmll->ll_head){                                  // Nodo cabecera.
            dmll->ll_head = current_node->next;
            dmll->ll_tail->next = dmll->ll_head;
        } else if (current_node == dmll->ll_tail){                                  // Nodo cola.
            dmll->ll_tail = previous_node;
            dmll->ll_tail->next = dmll->ll_head;
        } else {                                                                    // Nodo intermedio.
            previous_node->next = current_node->next;
        }

        // Liberación de memoria y eliminación del nodo:
        if (dmll->free_fn) dmll->free_fn(current_node->data);
        free(current_node);
        dmll->ll_len--;
        return DMLL_OK;

    } while (current_node != initial_node);

    return DMLL_NODE_NOT_FOUND;
}

/*
    @brief Función para eliminar un nodo, con función personalizada.
    @note: CDLL.

    @param dmll_t * dmll: Referencia a la estructura general de la lista enlazada.
    @param void * data: Datos del nodo a eliminar.
    @param bool (* cmp) (void *, void*): Función de comparación genérica.
    @retval dmll_error_t.
*/
static dmll_error_t dmll_delete_cdll(dmll_t * const dmll, void * const data, bool (* cmp_fn) (void *, void *)){
    // Comprobación de referencias:
    if (!dmll || !cmp_fn) return DMLL_INVALID_REFERENCE;
    if (!dmll->ll_head) return DMLL_EMPTY;

    // Nodo actual vs previo para comprobación:
    struct dmll_node * initial_node = dmll->ll_head;
    struct dmll_node * current_node = dmll->ll_head;

    // Comparación nodo a nodo:
    do {
        // Nodo no encontrado, se continúa con el bucle:
        if (!cmp_fn(current_node->data, data)){
            current_node = current_node->next;
            continue;
        }

        // Nodo encontrado, se actualizan punteros:
        if ((current_node == dmll->ll_head) && (current_node == dmll->ll_tail)){    // Nodo único.
            dmll->ll_head = NULL;
            dmll->ll_tail = NULL;
        } else if (current_node == dmll->ll_head){                                  // Nodo cabecera.
            dmll->ll_head = current_node->next;
            dmll->ll_head->prev = dmll->ll_tail;
            dmll->ll_tail->next = dmll->ll_head;
        } else if (current_node == dmll->ll_tail){                                  // Nodo cola.
            dmll->ll_tail = current_node->prev;
            dmll->ll_tail->next = dmll->ll_head;
            dmll->ll_head->prev = dmll->ll_tail;             
        } else {                                                                    // Nodo intermedio.
            current_node->prev->next = current_node->next;
            current_node->next->prev = current_node->prev;
        }

        // Liberación de memoria y eliminación del nodo:
        if (dmll->free_fn) dmll->free_fn(current_node->data);
        free(current_node);
        dmll->ll_len--;
        return DMLL_OK;

    } while (current_node != initial_node);

    return DMLL_NODE_NOT_FOUND;
}


/*
    @brief Función para encontrar un dato y retornar el nodo en la lista.

    @param dmll_t * const dmll: Referencia a la estructura general de la lista enlazada.
    @param void * data: Referencia al dato a buscar.
    @param bool (* cmp_fn)(void *, void *): Función personalizada con los criterios de comparación.
    @retval struct dmll_node *: Referencia al nodo encontrado.
*/
static struct dmll_node * dmll_find(dmll_t * const dmll, void * const data, bool (* cmp_fn)(void *, void *)){
    // Comprobación de referencias:
    if (!dmll || !cmp_fn) return NULL;

    // Bucle para buscar nodo a nodo:
    struct dmll_node * current = dmll->ll_head;
    if (!current) return NULL;

    do {
        // Comprobación de nodo:
        if (cmp_fn(current->data, data)) return current;
        current = current->next;
        
    } while (current && (current != dmll->ll_head));

    return NULL;
}

/*
    @brief Función para aplicar una función a los datos de todos los nodos.

    @param dmll_t * const dmll: Referencia a la estructura general de la lista enlazada.
    @param void (* fn)(void *): Función a aplicar en los datos de cada nodo.
    @retval dmll_error_t.
*/
static dmll_error_t dmll_foreach(dmll_t * const dmll, void (* fn)(void *)){
    // Comprobación de referencias:
    if (!dmll || !fn) return DMLL_INVALID_REFERENCE;

    // Bucle para recorrer nodo a nodo y aplicar la función dada:
    struct dmll_node * current = dmll->ll_head;
    if (!current) return DMLL_OK;

    do {
        fn(current->data);
        current = current->next;
    } while(current && (current != dmll->ll_head));

    return DMLL_OK;
}

