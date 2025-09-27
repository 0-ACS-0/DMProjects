#include "inc/dmcli_io.h"

 int main(int argc, char ** argv){
    dmcli_io_pt cli_io;
    dmcli_io_alloc(&cli_io);

    dmcli_io_enterm_rawmode(cli_io);
    while(true){
        dmcli_io_wait4input(cli_io);
        if (!strcmp(cli_io->input, "quit")) break;
    }
    dmcli_io_disterm_rawmode(cli_io);

    dmcli_io_dealloc(&cli_io);

    return 0;
 }