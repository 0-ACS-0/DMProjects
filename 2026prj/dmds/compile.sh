#!/bin/bash

# Variables de entorno             #
# -------------------------------- #
CC=gcc
CFLAGS="-g3 -Wall -O0"
CFLAGS_LIB="-std=c99 -Wall -O2"

SRC_DIR="./src/"
INC_DIR="./inc/"
BUILD_DIR="./build/"
SRC=$(find "$SRC_DIR" -name "*.c")
INC=$(find "$INC_DIR" -name "*.h")

LIB_NAME=libdmds
LIB_DIR="$BUILD_DIR"lib/
LIB_HDR=dmds.h
OBJ_DIR="$BUILD_DIR"obj/
OBJ_FILES=""
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
        exit 1
    fi

    # Compilación y ejecución con mensajes:
    echo "[BUILD-TEST]: Compilando programa de test $TEST_NAME..."
    if $CC $CFLAGS -I$INC_DIR $SRC $TEST_SRC -o $TEST_DIR$TEST_NAME; then
        echo "[BUILD-TEST]: Compilación finalizada con éxito en $TEST_DIR$TEST_NAME."
        echo "[BUILD-TEST]: Ejecutando programa de test $TEST_NAME..."
        echo
        $TEST_DIR$TEST_NAME
        echo
        echo "[BUILD-TEST]: Ejecución finalizada con código $?"
    else
        echo "[BUILD-ERR]: Error de compilación."
    fi
    echo

# CMD: lib 
elif [ "$1" == "lib" ]; then
    echo
    #Generación del directorio de objetos (bajo build) si no existe:
    if [ ! -d $OBJ_DIR ]; then
        echo "[BUILD-LIB]: Generando directorio $OBJ_DIR..."
        mkdir -p $OBJ_DIR
        echo "[BUILD-LIB]: Directorio generado."
        echo
    fi

    # Generación del directorio de librerías (bajo build) si no existe:
    if [ ! -d $LIB_DIR ]; then
        echo "[BUILD-LIB]: Generando directorio $LIB_DIR..."
        mkdir -p $LIB_DIR
        echo "[BUILD-LIB]: Directorio generado."
        echo
    fi

    # Copia del fichero de cabecera de la librería:
    if [ -f $INC_DIR$LIB_HDR ]; then
        echo "[BUILD-LIB]: Copiando fichero de cabecera de librería $INC_DIR$LIB_HDR..."
        cp $INC_DIR$LIB_HDR $LIB_DIR
        echo "[BUILD-LIB]: Fichero de cabecera copiado correctamente en $LIB_DIR$LIB_HDR."
        echo
    else
        echo "[BUILD-ERR]: No existe el fichero de cabecera $INC_DIR$LIB_HDR."
        exit 1
    fi

    # Compilación de la librería estática (.a):
    for file in $SRC; do
        obj="$OBJ_DIR$(basename "$file" .c).o"
        echo "[BUILD-LIB]: Compilando objeto $obj..."

        if $CC $CFLAGS_LIB -c -fPIC "$file" -I"$INC_DIR" -o "$obj"; then
            OBJ_FILES="$OBJ_FILES $obj"
        else
            echo "[BUILD-ERR]: Error compilando $file."
            exit 1
        fi

        echo "[BUILD-LIB]: Objeto $obj compilado con éxito."
    done
    echo "[BUILD-LIB]: Generando librería estática $LIB_NAME.a..."
    if ar rcs "$LIB_DIR$LIB_NAME".a $OBJ_FILES; then
        echo "[BUILD-LIB]: Librería estática generada en $LIB_DIR$LIB_NAME.a"
        echo
    else 
        echo "[BUILD-LIB]: Error generando $LIB_NAME.a"
        exit 1
    fi

    # Compilación de la librería dinámica (.so):
    echo "[BUILD-LIB]: Compilando librería dinámica $LIB_NAME.so..."
    if $CC $CFLAGS_LIB -shared $OBJ_FILES -o "$LIB_DIR$LIB_NAME".so; then
        echo "[BUILD-LIB]: Compilación finalizada con éxito en $LIB_DIR$LIB_NAME.so."
    else
        echo "[BUILD-ERR]: Error generando $LIB_NAME.so."
        exit 1
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