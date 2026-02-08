# Dmlogger user-guide
## Table of contents
- [Introduction](#introduction)
- [Build script](#build-script)
- [Set-up](#set-up)
- [Workflow](#workflow)

## Introduction
This, is **dmlogger**. A project made for logging messaages in a professional and easy way (it follows my personal tastes for logging libraries), that let the programmer have absolute control over the lib, and for my future projects.

Dmlogger is written in C, exclusively for linux OSs (in theory MACos too, but I didn't tested it). It is non-blocking, that means that the slow *write to disk* operations are running "at the same time" (is the OS who decides...) as the main program executes, without waiting the write operation to finish. But, as you will see in the workflow section, you can control this behaveour and how everything operates in a really easy way.

In the proyect there is a **build.sh** script to compile the library, test the *dmlogger_test.c* source or clean the workspace. I won't explain it here, but in case of modifying this library, it would be necessary to make use of it to recompile.

## Set-up (C)
Let's start using this now. Is as simple as this:

### In C:
1. Copy the *lib/* folder into your workspace, where you want to make use of it (you can rename the folder if *lib/* is too generic).

2. In your code, add the line ``` #include "./lib/dmlogger.h" ``` and you will have access to the logger functionality.

4. When you compile your program, link the library with ``` gcc [...] -L./lib/dmlogger -ldmlogger [...]```.

8. Debug your program, and have fun logging messages ^-^.

### In python:
1. Copy the *pywrapper/* folder into your python modules (or workspace if you aren't scared of looking at files).

2. Make sure that *libdmlogger.so.link* points directly to libdmlogger.so from the *lib/* folder (or modify *DMLogger.py* with the path where you want to store libdmlogger.so). 

3. Add the module into your python file: ``` from DMLogger import DMLogger ```

4. Pray to the python gods for it to work (jk, if you did that right it must work, but remember that this is a (in theory) unix specific module).


## Workflow
Now the fun part: using the logger!

Of course, the sintax is different in python than in C, but internally is the same code running behind. Anyways, I will explain the workflow of both methods.

### In C:
Here, after doing the set-up(C), you will have the power of dmlogger all for yourself. You can check ``` dmlogger_test.c ``` for a non-commented(I was tired :3) complete example of the logger. Of course, it follows a logical mental structure that I will explain:

- First, you need a variable that will contain the location of your logger. I defined the *dmlogger_pt* data type for that.
``` 
dmlogger_pt logger;
```
> Be aware that this is a pointer, so logger will be invalid until initialization.

- Second, you need to allocate the memory necessary for the logger to work (includes states, entry queue, configuration options...). Don't worry, I manage that for you with the init function.
``` 
dmlogger_init(&logger);
if (!logger) exit(1);
```
> Always check that logger is not NULL.

- Third, now you have allocated the logger but is not doing anythin. So, if you want all the logic running so loggin messages work, run it! After that, the logger won't do anything but will be prepared to log. 
```
    if(!dmlogger_run(logger)) exit(1);
``` 
> Please, check that this function succeeded.
If it succeeded you can start loggin things.

- Fourth, configure. You can configure it on the fly while running (is thread-safe and tested and doesn't delete any entry, but... yeah, be careful doing it). There is only four things to configure: output, queue overflow policy, queue capacity and minimum level. But every configuration has his own juice.

    1.