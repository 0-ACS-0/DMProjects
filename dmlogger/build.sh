#!/bin/bash


# Variables de entorno             #
# -------------------------------- #
CC=gcc
CFLAGS_TEST="-g -Wall -O2"
CFLAGS_LIB="-std=c99 -D_POSIX_C_SOURCE=200809L -shared -fPIC -Wall -Werror -O2 -lpthread"

LIB_SRC=dmlogger.c
INC=dmlogger.h
TEST_SRC="dmlogger.c dmlogger_test.c"

TEST_PROG=test.elf
LIB_PROG=dmlogger.so
# -------------------------------- #


# Lógica de uso                    #
# -------------------------------- #
if [ "$1" == "test" ]; then
    echo
    echo "[BUILD-TEST]: Compiling test program..."
    if $CC $CFLAGS_TEST $TEST_SRC -o $TEST_PROG; then
        echo "[BUILD-TEST]: Compilation completed!"
        echo "[BUILD-TEST]: Executing test program..."
        echo
        ./$TEST_PROG
        echo
        echo "[BUILD-TEST]: Finalized test program!"
    else
        echo "[BUILD-TEST ERR]: Compilatioin error, execution aborted."
    fi
    echo

elif [ "$1" == "lib" ]; then
    echo
    echo "[BUILD-LIB]: Compiling library..."
    if $CC $CFLAGS_LIB $LIB_SRC -o $LIB_PROG; then
        mv $LIB_PROG ./lib
        cp $INC ./lib
        echo "[BUILD-LIB]: Library compiled!."
    else
        echo "[BUILD-LIB ERR]: Compilation error, library not generated."
    fi
    echo

elif [ "$1" == "clean" ]; then
    echo
    echo "[BUILD-CLEAN]: Cleaning workspace..."
    rm -f ./$TEST_PROG ./lib/* ./logs/*
    echo "[BUILD-CLEAN]: Workspace completly clean!"
    echo

else
    echo
    echo "[BUILD ERR]: Incorrect use or invalid arguments:"
    echo -e "\n\t[Use]:"
    echo -e "\t\t-> ./build.sh test: \tCompile and execute the test program (.elf) under the ./ folder."
    echo -e "\t\t-> ./build.sh lib: \tCompile and generate the shared library (.so) under the ./lib/ folder."
    echo -e "\t\t-> ./build.sh clean: \tClean the workspace deleting generated files (including logs under ./logs/)."
    echo
    exit 1
fi
# -------------------------------- #