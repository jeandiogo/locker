# Locker

Locker is a single header C++20 library for Linux, providing a function that locks a file so it can be accessed exclusively or used for process synchronization (e.g. as an inter-process mutex).

The locking policy is only guaranteed among programs using this library. Locking a file does not prevent other processes from opening it, but it ensures that only one program will get the lock at a time. Once the lock has been acquired, one still has to open the file to read it and close it thereafter. The locker provides process-safety but not thread-safety, so one should use mutexes to synchronize its inner threads, and avoid forking a proccess while it has some file locked. A lockfile will be created if it does not exist, and it will be erased if it is empty at destruction. An exception will be throw if the file is invalid or unauthorized.

When compiling with g++, use the flag *-std=c++20* (available since GCC 10).

To compile and run the test, enter *make test* in the terminal.

## Usage:
```
#include "locker.hpp"
#include <fstream>

std::string const my_file = "my_file.txt";

int main()
{
    auto const my_lock = locker::lock_guard(my_file);
    
    auto my_stream = std::ofstream(my_file);
    
    //...
}
```
*Copyright (C) 2020 Jean "Jango" Diogo*
