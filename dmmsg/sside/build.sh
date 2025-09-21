#!/bin/bash

gcc -I./libs/dmserver -I./libs/dmlogger \
 dmmsg_sside.c \
 -L./libs/dmserver -L./libs/dmlogger \
 -ldmserver -ldmlogger \
 -Wl,-rpath=$(pwd)/libs/dmserver \
 -Wl,-rpath=$(pwd)/libs/dmlogger \
 -o dmmsg_server.elf
