#include "dmserver.h"

int main(int argc, char ** argv){
    dmserver_pt serv;
    dmserver_init(&serv);
    if (serv == NULL) exit(1);
    if(!dmserver_open(serv)) exit(1);
    if (!dmserver_run(serv)) exit(1);

    fgetc(stdin);

    if (!dmserver_stop(serv)) exit(1);
    if (!dmserver_close(serv)) exit(1);
    dmserver_deinit(&serv);
    return 0;
}