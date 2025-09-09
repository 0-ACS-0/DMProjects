#!/bin/bash
#openssl s_client -connect 192.168.1.38:2020 -ign_eof -no_ssl3
# Variables de entorno             #
# -------------------------------- #
CC=gcc
CFLAGS_TEST="-g -Wall -O0 -lssl -lcrypto -lpthread"
CFLAGS_LIB="-std=c99 -D_POSIX_C_SOURCE=200809L -shared -fPIC -Wall -Werror -O2 -lpthread -lssl -lcrypto"

LOGS_DIR="./logs"
LIBS_DIR="./libs"
SRC_DIR="./src"
INC_DIR="./inc"
INC="$(find $LIBS_DIR -name '*.h') $(find $INC_DIR -name '*.h')"
LIB_SRC="$(find $LIBS_DIR -name 'lib*.so') $(find $SRC_DIR -name '*.c')"
TEST_SRC="$LIB_SRC ./dmserver_test.c"

TEST_PROG=test.elf
PROG_HDR=dmserver.h
LIB_HDR=$INC_DIR/$PROG_HDR
LIB_PROG=libdmserver.so
# -------------------------------- #


# Lógica de uso                    #
# -------------------------------- #
if [ "$1" == "test" ]; then
    echo
    echo "[BUILD-TEST]: Compiling test program..."
    if $CC $CFLAGS_TEST -I$LIBS_DIR -I$INC_DIR $TEST_SRC -o $TEST_PROG; then
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
    if $CC $CFLAGS_LIB -I$INC_DIR -I$LIBS_DIR $LIB_SRC -o $LIB_PROG; then
        mv $LIB_PROG $LIBS_DIR
        cp $LIB_HDR $LIBS_DIR
        echo "[BUILD-LIB]: Library compiled!."
    else
        echo "[BUILD-LIB ERR]: Compilation error, library not generated."
    fi
    echo

elif [ "$1" == "clean" ]; then
    echo
    echo "[BUILD-CLEAN]: Cleaning workspace..."
    rm -f  $LOGS_DIR/* $LIBS_DIR/$LIB_PROG $LIBS_DIR/$PROG_HDR ./$TEST_PROG
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