# Locker

Locker is a single header C++20 class with static member functions to lock files on Linux systems, so they can be accessed exclusively or used for process synchronization (e.g. as slow inter-process mutexes).

- **The locking policy is guaranteed only among programs using this library.** Locking a file does not prevent other processes from opening it, but it ensures that only one program will get the lock at a time. All locking and unlocking functions accept a single filename, multiple filenames, a list of filenames, or a vector of filenames.

- **If the file to be locked does not exist it will be created.** However, an exception will be throw if an empty filename is given, if a directory name is given, or if the program does not have permission to read and write to the file and its directory.

- **The lockings are reentrant.** If you have locked a file twice, you have to unlock it twice too. Therefore, always prefer using the lock guard, which will automatically release a lockfile before leaving its scope of declaration.

- **The locker provides process-safety, but not thread-safety.** Once a process has acquired the lock, its threads and future forks will not be stopped by it nor they will be able to mutually exclude each other by using the filelock. Therefore, avoid forking a program while it has some file locked and use mutexes to synchronize its inner threads.

- **Lock and unlock operations are independent from open and close operations.** If you want to open a lockfile you need to use file handlers like "fopen" and "fstream", and close the file before the unlock. To circumvent that, this library provides functions for exclusive read, write, append, and memory-map, which are all process-safe (although still not thread-safe) and will not interfere with your current locks. It is still your responsability to handle race conditions among threads trying to open files locked by their parent.

When compiling with g++, use the flag *-std=c++20* (available since GCC 10).

To compile and run the test, enter *make test* in the terminal.

## Usage:
```
#include "locker.hpp"

bool success = locker::try_lock("a.lock");                               //tries to lock a file once, returns immediately
bool success = locker::try_lock("a.lock", "b.lock", "c.lock");           //tries to lock multiple files once, returns immediately
bool success = locker::try_lock({"a.lock", "b.lock"});                   //tries to lock a initializer list or a vector of files once, returns immediately

locker::lock("a.lock");                                                  //keeps trying to lock a file, only returns when file is locked
locker::lock("a.lock", "b.lock", "c.lock", "d.lock");                    //keeps trying to lock multiple files, only returns when all file are locked
locker::lock({"a.lock", "b.lock"});                                      //keeps trying to lock a initializer list or a vector of files, only returns when all files are locked

locker::unlock("a.lock");                                                //unlocks a file if it is locked (throws if file does not exist)
locker::unlock("a.lock", "b.lock");                                      //unlocks a multiple files (in reverse order) if they are locked
locker::unlock({"a.lock", "b.lock", "c.lock"});                          //unlocks a initializer list or a vector of files (in reverse order) if they are locked

locker::lock_guard_t my_lock = locker::lock_guard("a.lock");             //locks a file and automatically unlocks it before leaving current scope
locker::lock_guard_t my_lock = locker::lock_guard("a.lock", "b.lock");   //locks multiple files and automatically unlocks them before leaving current scope
locker::lock_guard_t my_lock = locker::lock_guard({"a.lock", "b.lock"}); //locks a initializer list or a vector of files and automatically unlocks them before leaving current scope

std::string my_data = locker::xread("a.txt");                            //exclusively reads a file (throws if file does not exist) and returns its content as a string (trailing newlines are removed)
std::vector<char> my_data = locker::xread<char>("a.txt");                //same, but does not remove trailing newlines and return content as a vector of user specified type
std::vector<int> my_data = locker::xread<int>("a.txt");                  //note that trailing bytes will be ignored if the file size is not a multiple of the chosen type size
std::vector<long> my_data = locker::xread<long>("a.txt");                //also note that traling newlines may be included if they turn the file size into a multiple of the type size
locker::xread("a.txt", my_container);                                    //open input file exclusively as a std::fstream and call the extraction operator from it to the container passed as argument

locker::xwrite("a.txt", my_data);                                        //exclusively writes formatted data to a file (data type must be insertable to std::fstream)
locker::xwrite("a.txt", "value", ':', 42);                               //exclusively writes multiple data to a file
locker::xwrite<true>("a.txt", "order", ':', 66);                         //use first template argument to append data instead of overwrite
locker::xwrite<false, true>("a.txt", "foobar");                          //use second template argument to write a trailing newline
	
locker::xflush("a.txt", my_vector);                                      //exclusively writes binary data to a file
locker::xflush<true>("a.txt", my_vector);                                //use template argument to append data instead of overwrite
locker::xflush("a.txt", my_data_pointer, my_data_size);                  //one can also send a raw void pointer and the length in bytes of the data to be written

locker::memory_map_t my_map = locker::xmap("a.txt");                     //exclusively maps a file to memory and returns a container that behaves like an array, throws if file does not exist or is not a regular file
locker::memory_map_t my_map = locker::xmap<char>("a.txt");               //the type underlying the array can be designated at instantiation via template argument, default is unsigned char
locker::memory_map_t my_map = locker::xmap<int>("a.txt");                //note that trailing bytes will be ignored if size of file is not a multiple of the size of the designated type
unsigned char my_var = my_map.at(N);                                     //gets the N-th memory position of the file based on the size of designated type, throws if N excedess file's range
unsigned char my_var = my_map[N];                                        //same, but does not check range
my_map.at(N) = M;                                                        //assigns the value M to the N-th position, throws if N excedess file's range
my_map[N] = M;                                                           //same, but does not check range
std::size_t my_size = my_map.get_size();                                 //gets the size of the array (which is size of file divided by size of the array underlying type)
std::size_t my_size = my_map.size();                                     //same as above, for STL compatibility
bool is_empty = my_map.is_empty();                                       //returns true if map is ampty
bool is_empty = my_map.empty();                                          //same as above, for STL compatibility
unsigned char * my_data = my_map.get_data();                             //gets a raw pointer to file's data, whose underlying type is unsigned char (or the one designated at instantiation)
unsigned char * my_data = my_map.data();                                 //same as above, for STL compatibility
my_map.flush();                                                          //flushes data to file (unnecessary, since current process will be the only one accessing the file)

bool success = locker::is_locked("a.txt");                               //returns true if file is currently locked, false otherwise (throws if file does not exist)
locker::clear();                                                         //unlocks all currently locked files (do not call this function if a lockfile is open)
```
*Copyright (c) 2020 Jean Diogo ([Jango](mailto:jeandiogo@gmail.com))*
