#!/bin/bash

# Variables de entorno             #
# -------------------------------- #
CC=gcc
CFLAGS_TEST="-g -Wall -O0"
CFLAGS_LIB="-std=c99 -D_POSIX_C_SOURCE=200809L -shared -fPIC -Wall -Werror -O2"

LIB_DIR="./lib"
SRC_DIR="./src"
INC_DIR="./inc"

INC="$(find $INC_DIR -name '*.h')"
SRC=$(find $SRC_DIR -name '*.c')
TEST_SRC="$SRC ./dmcli_test.c"
LIB_HDR=$(find $INC_DIR -name '*.h')

TEST_PROG=test.elf
LIB_PROG=libdmcli.so
# -------------------------------- #


# Lógica de uso                    #
# -------------------------------- #
if [ "$1" == "test" ]; then
    echo
    echo "[BUILD-TEST]: Compiling test program..."
    if $CC -I$INC_DIR $CFLAGS_TEST $TEST_SRC -o $TEST_PROG; then
        echo "[BUILD-TEST]: Compilation completed!"
        echo "[BUILD-TEST]: Executing test program..."
        echo

        ./$TEST_PROG
        TEST_EXIT=$?

        echo
        echo "[BUILD-TEST]: Finalized test program with exit code: $TEST_EXIT"
    else
        echo "[BUILD-TEST ERR]: Compilatioin error, execution aborted."
    fi
    echo

elif [ "$1" == "lib" ]; then
    echo
    echo "[BUILD-LIB]: Compiling library..."
    if $CC $CFLAGS_LIB -I$INC_DIR $SRC -o $LIB_PROG; then
        mv $LIB_PROG $LIB_DIR
        cp $LIB_HDR $LIB_DIR
        echo "[BUILD-LIB]: Library compiled!."
    else
        echo "[BUILD-LIB ERR]: Compilation error, library not generated."
    fi
    echo

elif [ "$1" == "clean" ]; then
    echo
    echo "[BUILD-CLEAN]: Cleaning workspace..."
    rm -f $LIB_DIR/* ./$TEST_PROG
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