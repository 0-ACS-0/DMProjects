# Dmserver user-guide
## Table of contents
- [Introduction](#introduction)
- [Build script](#bscript)
- [Getting started](#gstarted)

## Introduction
This, is **dmserver**. A fun-to-make proyect made from scratch with the only purpose of learning and creating a fun tool to use and have control over for my future self. 

Dmserver consist in a high performance and easy to use server, with no "application layer" (dmserver is application layer by definition, but grant further freedom to the programmer). Is written in C, exclusively for linux OSs. This version is limited and includes a very restrictive configuration options.

Only the library folder is necessary to export and use this proyect in another proyects. This library contains all headers and the shared object "libdmserver.so", so the path of this folder must be known by the compiler of the proyects that make use of it.

In the next chapter is explained step by step how to use the bash script created to build and test dmserver, in case of modifing dmserver itself. **Not** to work with the library.

## Build script
> Note: This section is only necessary if you want to modify the library and thus dmserver itself. If your purpose is only to use it, skip this section. 

### Basic usage.
> Note: In order to work with this script (without modification) is essential to keep the proyect folder trees as is.

If you are working with the *dmserver/* proyect, you will probably see the bash script called *build.sh*. This is a small (very small) script I made to fastly **test**, **clear** and **make the library** for dmserver, without issuing any further complex compiler call.

First of all, make sure to check if this script has execution rights. You can do it in linux by typing:
```
chmod +x ./build.sh
```
Now, you will have three options to use this script:

1. ``` ./build test```: Which will compile the whole library alongside with the source file called *dmserver_test.c*, which will contain the test program. After that, the test program compiled will be executed automatically. Finally, when the program terminates, the script will show you the exit status of the termination.

2. ``` ./build clean ```: This will clean the workspace, meaning that all the files produced with this script will be removed. All libraries, logs and test executable included, so be aware of the risks.

3. ``` ./build lib ```: This will compile and generate the directory *dmserver/* behind *libs/*, which will contain all the header files and the *.so* necessary to export and work with the library externally.

Feel free to modify it to your necessities. At the begining of the script you will spot all the local variables, try changing some if you know what you are doing. Again, be careful with the *clean* option, so it won't ask twice before deleting anything that the script considers it's own files.

If you want to further develope this proyect, mantain all headers inside *inc/* dir, and all source code inside *src/* dir, so it will be automatically included in the compilation process (test and lib). Note that the test option is for testing the library, **not** to compile a proyect made by it. 

> /!\ Note /!\ : If you recompile the library or modify it, and due to a path error, it will be necessary to modify manually the path of the server logger. In *_dmserver_hdrs.h*, under *Logging system* comment, delet *libs* from the path and it will be fixed (it is possible to fix this directly in the build script, but I am tired ;3).

### Test source.
The ``` ./build test ``` takes the file *dmserver_test.c* as the source code to test. Right now is a simple example of usage of the library (I think is clear enough to just read the code and understand the usage), and a little *cli* to interact with the server in real time while running and understand it's behaveour.

But this is only an example, and you can modify everything freely. The structure followed in this test source is:

- Library include.
- Global server variable.
- Callbacks functions prototypes.
- Main function.
- Callbacks functions implementations.

I recommend this as a footprint for further testing.

## Getting started