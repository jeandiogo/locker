# Locker

Locker is a single header C++20 library for Linux programs, providing functions to lock files so they can be accessed exclusively or used for process synchronization (e.g. as slow inter-process mutexes).

- **The locking policy is guaranteed only among programs using this library.** Locking a file does not prevent other processes from opening it, but it ensures that only one program will get the lock at a time.

- **If the file to be locked does not exist it will be created.** However, an exception will be throw if an empty filename is given, if a directory name is given, or if the program does not have permission to read and write to the file and its directory. If the lockfile is empty during the unlock, it will be erased.

- **The locker provides process-safety, but not thread-safety.** Once a process has acquired the lock, its threads will not be stopped by it, nor they will be able to mutually exclude each other using the locker. The same is not truth for forks of the process. Since its children will be different processes, they must use the locker to prevent themselves from accessing a file already locked by their parent, as well as they will have to wait until the parent releases the lock. For this reason, avoid forking a proccess while it has some file locked, and use mutexes to synchronize its inner threads.

- **Lock and unlock operations are independent from open and close operations.** If you want to open a lockfile you need to use file handlers like "fopen" and "fstream", and close the file before unlock. To circumvent that, this library provides functions for exclusive read, write, and append, which are all process-safe (although not thread-safe) and will not interfere with your current locks. It is still your responsability to handle race conditions among threads trying to open files locked by their parent, and to prevent the deadlock of child processes waiting to lock a file already locked by its parent.

When compiling with g++, use the flag *-std=c++20* (available since GCC 10).

To compile and run the test, enter *make test* in the terminal.

## Usage:
```
#include "locker.hpp"

locker::lock_guard_t my_lock = locker::lock_guard("a.lock");              //locks a file and automatically unlocks it before leaving current scope (an empty lockfile will be created if it does not exist)
locker::lock_guard_t my_lock = locker::lock_guard<true>("a.lock");        //use first template argument to make it "non-blocking" (i.e. will throw instead of wait if file is already locked)
locker::lock_guard_t my_lock = locker::lock_guard<false, true>("a.lock"); //use second template argument to not delete empty lockfiles (by default empty lockfiles are erased at destruction)

std::string       my_data = locker::xread("a.txt");                       //exclusively formatted data from a file and returns its content as a string (returns an empty string if file does not exist)
std::string       my_data = locker::xread<true>("a.txt");                 //use template argument to remove trailing newlines ("\n" and "\r\n")
std::vector<char> my_data = locker::xread<char>("a.txt");                 //use template typename to get file content as a vector of some specified type
std::vector<int>  my_data = locker::xread<int>("a.txt");                  //note that in these cases trailing bytes will be ignored if the size of the file is not a multiple of the size of the chosen type
std::vector<long> my_data = locker::xread<long>("a.txt");                 //also note that an eventual traling newline may be included if it turns the file size into a multiple of the type size
locker::xread("a.txt", my_container);                                     //exclusively inserts data from a file (std::fstream) into a container via a single call of the ">>" operator

locker::xwrite("a.txt", "content");                                       //exclusively writes formatted data
locker::xwrite("a.txt", "value", ':', 42);                                //exclusively writes multiple data to file
locker::xwrite<true>("a.txt", "order", ':', 66);                          //use first template argument to append data instead of overwrite
locker::xwrite<false, true>("a.txt", "foobar");                           //use second template argument to write a trailing newline
locker::xwrite("a.txt", my_data);                                         //exclusively inserts data from a container into a file (std::fstream) via a single call of the "<<" operator

locker::xflush("a.txt", my_vector);                                       //exclusively writes binary data from a std::vector to a file
locker::xflush("a.txt", my_span);                                         //same as above, but with an std::span instead of a vector
locker::xflush("a.txt", my_data_pointer, my_data_size);                   //you can also send a raw void pointer to the data, and its length in bytes
locker::xflush<true>("a.txt", my_vector);                                 //use template argument to append data instead of overwrite

locker::xremove("filename");                                              //locks a file, then removes it
```
*Copyright (C) 2020 Jean "Jango" Diogo*
