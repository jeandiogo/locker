# Locker

Locker is a header-only C++20 class with static member functions to lock files in Linux systems, so they can be accessed exclusively or used as inter-process mutexes.

The locking policy works only among programs using this library, so locking a file does not prevent other processes from opening it, but it ensures that only one program will get the lock at a time. Moreover, **the locker does not provide thread-safety**. Once a process has acquired the lock, neither its threads and future forks will be stopped by it, nor they will be able to mutually exclude each other by using the filelock. Therefore, avoid forking a program while it has some file locked, and use ordinary mutexes to synchronize its inner threads.

An exception will be throw if an empty filename is given, if a directory name is given, or if the program does not have permission to read from and write to the file and its directory. **If the file to be locked does not exist, it will be created**. All locking and unlocking functions are variadic, accepting a single filename, multiple filenames, a list of filenames, or a vector of filenames. If you have manually locked a file, do not forget to unlock it. Nevertheless, prefer using the lock guard, which will automatically unlock the file before leaving its scope of declaration.

Be aware that **lock and unlock operations are independent from open and close operations**. If you want to open a lockfile, you need to use file handlers like "fstream" or "fopen", and close the file before unlocking it. It is also your responsability to handle race conditions among threads that have opened a file locked by their parent. Instead of manually locking and opening a file, we suggest using the functions this library provides to perform exclusive read, write, append, and memory-map, which are all process-safe (although still not thread-safe) and will not interfere with your current locks.

Finally, **a process will loose the lock if the lockfile is deleted**. So it may be a good practice to create separate lockfiles for each file you intend to use (e.g. to exclusively open "a.txt", lock the file "a.txt.lock"). This will prevent you from losing the lock in case you need to erase and recreate the file without losing the lock to other processes. Do not forget to be consistent with the name of lockfiles throughout your programs.

*When compiling with g++ use the flag "-std=c++2a" (available in GCC 7.0 or later).*

## Usage:

	#include "locker.hpp"
	
	bool success = locker::try_lock("a.lock");                               //tries to lock a file once, returns immediately
	bool success = locker::try_lock("a.lock", "b.lock");                     //tries to lock multiple files once, returns immediately
	bool success = locker::try_lock({"a.lock", "b.lock"});                   //same as above

	locker::lock("a.lock");                                                  //keeps trying to lock a file, only returns when file is locked
	locker::lock("a.lock", "b.lock");                                        //keeps trying to lock multiple files, only returns when files are locked
	locker::lock({"a.lock", "b.lock"});                                      //same as above

	locker::lock(1, "a.lock");                                               //keeps trying to lock in intervals of approximately 1 millisecond
	locker::lock<std::chrono::nanoseconds>(1000, "a.lock");                  //use template argument to change the unit of measurement
	locker::lock(20, "a.lock", "b.lock");                                    //also works for the variadic versions

	locker::unlock("a.lock");                                                //unlocks a file if it is locked
	locker::unlock("a.lock", "b.lock");                                      //unlocks multiple files (in reverse order) if they are locked
	locker::unlock({"a.lock", "b.lock"});                                    //same as above

	locker::lock_guard_t my_lock = locker::lock_guard("a.lock");             //locks a file and automatically unlocks it before leaving current scope
	locker::lock_guard_t my_lock = locker::lock_guard("a.lock", "b.lock");   //locks multiple files and automatically unlocks them before leaving current scope
	locker::lock_guard_t my_lock = locker::lock_guard({"a.lock", "b.lock"}); //same as above

	std::string my_data = locker::xread("a.txt");                            //exclusively reads a file and returns its content as a string
	std::string my_data = locker::xread<true>("a.txt");                      //same but does not unlock the file at destruction (use this if the file was already lock before the call)

	locker::xwrite("a.txt", my_data);                                        //exclusively writes data to a file (data type must be insertable to std::fstream)
	locker::xwrite("a.txt", "value", ':', 42);                               //exclusively writes multiple data to a file
	locker::xwrite<true>("a.txt", my_data);                                  //same but does not unlock the file at destruction (use this if the file was already lock before the call)

	locker::xappend("a.txt", my_data);                                       //exclusively appends data to a file (data type must be insertable to std::fstream)
	locker::xappend("a.txt", "value", ':', 42);                              //exclusively appends multiple data to a file
	locker::xappend<true>("a.txt", my_data);                                 //same but does not unlock the file at destruction (use this if the file was already lock before the call)

	locker::memory_map_t my_map = locker::xmap("a.txt");                     //exclusively maps a file to memory and returns a structure with a pointer to an array of unsigned chars
	locker::memory_map_t my_map = locker::xmap<true>("a.txt");               //same but does not unlock the file at destruction (use this if the file was already lock before the call)
	unsigned char my_var = my_map.at(n);                                     //gets the n-th byte as an unsigned char, throws file's content is smaller than n bytes
	unsigned char my_var = my_map[n];                                        //same, but does not check range
	my_map.at(10) = m;                                                       //assigns the value m to the n-th byte, throws if file's content is smaller than n bytes
	my_map[10] = m;                                                          //same, but does not check range
	std::size_t my_size = my_map.size();                                     //gets size of file's content (which is one byte less than the size of the file)
	unsigned char * my_array = my_map.data();                                //gets raw pointer to file's content, represented as an array of unsigned chars
	my_map.flush();                                                          //flushes data to file (unnecessary, since OS handles it automatically)

*Copyright 2020 Jean Diogo ([Jango](mailto:jeandiogo@gmail.com))*
