#include "./inc/dmds.h"

/*---- Prototipos de funcioens de prueba ----*/

/*---- MAIN ----*/
int main(int argc, char ** argv){
    return 0;
}

/*---- Funciones de prueba ----*/
void test_dmll(void){
    dmll_t * dmll;
    if (dmll_init(&dmll, &(dmll_conf_t){.node_type=DMSLL_NODE}) != DMLL_OK) exit(1);
}