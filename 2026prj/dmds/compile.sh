#!/bin/bash

# Variables de entorno             #
# -------------------------------- #
CC=gcc
CFLAGS="-g3 -Wall -O0"
CFLAGS_LIB="-std=c99 -shared -fPIC -Wall -O2"

SRC_DIR="./src/"
INC_DIR="./inc/"
BUILD_DIR="./build/"
SRC=$(find "$SRC_DIR" -name "*.c")
INC=$(find "$INC_DIR" -name "*.h")

LIB_NAME=libdmdtypes.so
LIB_DIR="$BUILD_DIR"lib/
TEST_DIR="$BUILD_DIR"test/
# -------------------------------- #

# Lógica de operación              #
# -------------------------------- #
# Creación del directorio "build" en el espacio de trabajo.
if [ ! -d $BUILD_DIR ]; then
    echo 
    echo "[BUILD]: Generando directorio $BUILD_DIR..."
    mkdir -p $BUILD_DIR
    echo "[BUILD]: Directorio generado."
fi

# CMD: test
if [ "$1" == "test" ]; then
    echo
    # Generación del directorio test (bajo build) si no existe:
    if [ ! -d $TEST_DIR ]; then
        echo "[BUILD-TEST]: Generando directorio $TEST_DIR..."
        mkdir -p $TEST_DIR
        echo "[BUILD-TEST]: Directorio generado."
        echo
    fi

    # Comprobación de nombre del test (segundo argumento):
    if [ "$2" ]; then
        TEST_SRC="$2".c
        TEST_NAME="$2".elf
    else 
        echo "[BUILD-ERR]: No se ha indicado el nombre del código fuente del test."
        echo
        exit
    fi

    # Compilación y ejecución con mensajes:
    echo "[BUILD-TEST]: Compilando programa de test $TEST_NAME..."
    if $CC $CFLAGS -I$INC_DIR $SRC $TEST_SRC -o $TEST_DIR$TEST_NAME; then
        echo "[BUILD-TEST]: Compilación finalizada con éxito en $TEST_DIR$TEST_NAME."
        echo "[BUILD-TEST]: Ejecutando programa de test $TEST_NAME..."
        $TEST_DIR$TEST_NAME
        echo "[BUILD-TEST]: Ejecución finalizada con código $?"
    else
        echo "[BUILD-ERR]: Error de compilación."
    fi
    echo

# CMD: lib 
elif [ "$1" == "lib" ]; then
    echo
    # Generación del directorio de librería (bajo build) si no existe:
    if [ ! -d $LIB_DIR ]; then
        echo "[BUILD-LIB]: Generando directorio $LIB_DIR..."
        mkdir -p $LIB_DIR
        echo "[BUILD-LIB]: Directorio generado."
        echo
    fi

    echo "[BUILD-LIB: Compilando librería $LIB_NAME..."
    if $CC $CFLAGS_LIB $SRC -o $LIB_DIR$LIB_NAME; then
        echo "[BUILD-LIB]: Compilación finalizada con éxito en $LIB_DIR$LIB_NAME."
    else
        echo "[BUILD-ERR]: Error de compilación."
    fi
    echo

# CMD: clean
elif [ "$1" == "clean" ]; then
    echo
    echo "[BUILD-CLEAN]: Eliminando directorio y subdirectorios $BUILD_DIR..."
    rm -rf $BUILD_DIR
    echo "[BUILD-CLEAN]: Directorio eliminado."
    echo

else
    echo
    echo "[BUILD-ERR]: Uso incorrecto del comando."
    echo -e "\nComo utilizar compile.sh:"
    echo -e "\t-> ./compile.sh test {x} \tCompila y ejecuta el programa de test {x} bajo $TEST_DIR$TEST_NAME"
    echo -e "\t-> ./compile.sh lib \t\tCompila el código fuente en la librería $LIB_DIR$LIB_NAME"
    echo -e "\t-> ./compile.sh clean \t\tElimina el directorio $BUILD_DIR y su contenido."
    echo
    exit 1
fi
# -------------------------------- #