# Locker

Locker is a single header C++20 library for Linux, providing a function that locks a file so it can be accessed exclusively or used for process synchronization (e.g. as a slow inter-process mutex).

The locking policy is only valid to programs using this library. Locking a file does not prevent other processes from opening it, but ensures that only one program will get the lock at a time. The locker provides process-safety but not thread-safety, so one should use mutexes to synchronize its inner threads, and avoid forking a proccess while it has some file locked. Once the lock has been acquired, one still has to open the file to read it and close it thereafter. An exception will be throw if the file is invalid or unauthorized. A lockfile will be created if it does not exist, and it will be erased if it is empty at destruction.

When compiling with g++, use the flag *-std=c++20* (available since GCC 10).

To compile and run the test, enter *make test* in the terminal.

## Usage:
```
#include "locker.hpp"
#include <fstream>

int main()
{
    auto const filename = "my_file.txt";
    
    auto const lock = locker::lock_guard(filename);
    
    //you can access the file exclusively from here until the end of this scope
    
    auto stream = std::ofstream(filename);
    
    //...
}
```
*Copyright (C) 2020 Jean "Jango" Diogo*
