# Locker

Locker is a single header C++20 library for Linux, providing a function that locks a file so it can be accessed exclusively or used for process synchronization (e.g. as a slow inter-process mutex).

The locking policy is guaranteed only to programs using this library. Locking a file does not prevent other processes from opening it, but it ensures that only one program will get the lock at a time. The locker provides process-safety but not thread-safety, so one should use mutexes to synchronize its inner threads and avoid forking a proccess while it has some file locked. Once the lock has been acquired, one still has to open the file to read it and close it after read (or use the auxiliary process-safe functions also available in this library). An exception will be throw if the file is invalid or unauthorized. A lockfile will be created if it does not exist, and will be erased if it is empty at destruction.

When compiling with g++, use the flag *-std=c++20* (available since GCC 10).

To compile and run the test, enter *make test* in the terminal.

## Usage:
```
#include "locker.hpp"

locker::lock_guard_t my_lock = locker::lock_guard("a.lock");              //locks a file and automatically unlocks it before leaving current scope (an empty lockfile will be created if it does not exist)
locker::lock_guard_t my_lock = locker::lock_guard<true>("a.lock");        //use first template argument to make it "non-blocking" (i.e. will throw instead of wait if file is already locked)
locker::lock_guard_t my_lock = locker::lock_guard<false, true>("a.lock"); //use second template argument to not erase empty lockfiles (by default empty lockfiles are erased at destruction)

std::string          my_data = locker::read("a.txt");                     //exclusively reads text file and returns it as string (returns empty string if file dont exist)
std::string          my_data = locker::read<true>("a.txt");               //use template argument to remove trailing newlines ("\n" and "\r\n")

std::vector<char>    my_data = locker::read<char>("a.txt");               //use template typename to read binary file as a vector of some specified type
std::vector<int>     my_data = locker::read<int>("a.txt");                //note that trailing bytes will be ignored if file size is not multiple of type size
std::vector<long>    my_data = locker::read<long>("a.txt");               //note that traling newlines may be included if they make file size multiple of type size
locker::read("a.txt", my_container);                                      //reads from std::fstream to container via single call of ">>" operator

locker::write("a.txt", "order", ':', 66);                                 //exclusively writes formatted data to file
locker::write<true>("a.txt", "foobar");                                   //use first template argument to append data instead of overwrite
locker::write<false, true>("a.txt", "foobar");                            //use second template argument to write a trailing newline
locker::write("a.txt", my_container);                                     //writes from container to std::fstream via single call of "<<" operator

locker::flush("a.txt", my_vector_or_my_span);                             //exclusively writes binary data to file
locker::flush<true>("a.txt", my_vector_or_my_span);                       //use template argument to append instead of overwrite

locker::copy("file_1", "file_2");                                         //locks a file, then copies it to another
locker::move("file_1", "file_2");                                         //locks a file, then renames it to another
locker::remove("filename");                                               //locks a file, then removes it
```
*Copyright (C) 2020 Jean "Jango" Diogo*
