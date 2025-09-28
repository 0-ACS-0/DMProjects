#include "inc/dmcli.h"

 int main(int argc, char ** argv){
    dmcli_t cli = {0};
    dmcli_init(&cli);
    dmcli_loop(&cli);
    return 0;
 }