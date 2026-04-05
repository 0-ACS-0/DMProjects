#include "./inc/dmds.h"
#include <stdio.h>

/*---- Prototipos de funcioens de prueba ----*/
void test_dmll(void);

/*---- MAIN ----*/
int main(int argc, char ** argv){
    test_dmll();
    return 0;
}

/*---- Funciones de prueba ----*/
void funfunfun(void * data){
    printf("%c\r\n", *(char *)data);
}

void test_dmll(void){
    dmll_t * dmll;
    if (dmll_init(&dmll, &(dmll_conf_t){.node_type=DMSLL_NODE, .free_fn=NULL}) != DMLL_OK) exit(1);
    char c = 'a';
    dmll->fn.dmll_insert(dmll, &c);
    dmll->fn.dmll_insert(dmll, &c);
    dmll->fn.dmll_insert(dmll, &c);
    dmll->fn.dmll_foreach(dmll, funfunfun);
    dmll_deinit(&dmll);
}

