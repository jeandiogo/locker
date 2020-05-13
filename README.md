# Locker

Locker is a header-only C++20 class with static member functions to lock files in Linux systems, so they can be accessed exclusively or used as inter-process mutexes.

The locking policy works only among programs using this library, so locking a file does not prevent other processes from opening it, but it ensures that only one program will get the lock at a time. Moreover, **the locker does not provide thread-safety**. Once a process has acquired the lock, neither its threads and future forks will be stopped by it, nor they will be able to mutually exclude each other by using the filelock. Therefore, avoid forking a program while it has some file locked, and use ordinary mutexes to synchronize its inner threads.

An exception will be throw if an empty filename is given, if a directory name is given, or if the program does not have permission to read from and write to the file and its directory. **If the file to be locked does not exist, it will be created**. All locking and unlocking functions are variadic, accepting a single filename, multiple filenames, a list of filenames, or a vector of filenames. If you have manually locked a file, do not forget to unlock it. Nevertheless, prefer using the lock guard, which will automatically unlock the file before leaving its scope of declaration.

Be aware that **lock and unlock operations are independent from open and close operations**. If you want to open a lockfile, you need to use file handlers ("fstream", "fopen" etc.) and close the file before unlocking it. It is also your responsability to handle race conditions among threads that have opened a file locked by their parent. If you prefer, instead of manually locking and opening a file, use the functions this library provides to perform exclusive read and exclusive write, which are process-safe (but still not thread-safe).

Finally, **you will lose the lock if a lockfile is deleted**. So it may be a good practice to create separate lockfiles for each file you intend to use (e.g. to exclusively open "a.txt", lock the file "a.txt.lock"). This will prevent you from losing the lock in case you need to erase and recreate the file without losing the lock to other processes. Do not forget to be consistent with the name of lockfiles throughout your programs.

*When compiling with g++ use the flag "-std=c++2a" (available in GCC 7.0 or later).*

## Usage:

	#include "locker.hpp"
	
	bool success = locker::try_lock("a.lock");                 //tries to lock a file once, returns immediately
	bool success = locker::try_lock("a.lock", "b.lock");       //tries to lock multiple files once, returns immediately
	bool success = locker::try_lock({"a.lock", "b.lock"});     //same as above
		
	locker::lock("a.lock");                                    //keeps trying to lock a file, only returns when file is locked
	locker::lock("a.lock", "b.lock");                          //keeps trying to lock multiple files, only returns when files are locked
	locker::lock({"a.lock", "b.lock"});                        //same as above
	
	locker::lock(1, "a.lock");                                 //keeps trying to lock in intervals of approximately 1 millisecond
	locker::lock<std::chrono::nanoseconds>(1000, "a.lock");    //use template argument to change the unit of measurement
	locker::lock(20, "a.lock", "b.lock");                      //also works for the variadic versions
	
	locker::unlock("a.lock");                                  //unlocks a file if it is locked
	locker::unlock("a.lock", "b.lock");                        //unlocks multiple files (in reverse order) if they are locked
	locker::unlock({"a.lock", "b.lock"});                      //same as above
		
	auto my_lock = locker::lock_guard("a.lock");               //locks a file and automatically unlocks it before leaving current scope
	auto my_lock = locker::lock_guard("a.lock", "b.lock");     //locks multiple files and automatically unlocks them before leaving current scope
	auto my_lock = locker::lock_guard({"a.lock", "b.lock"});   //same as above
		
	bool success = locker::is_locked("a.lock");                //asserts if a file is already locked by current process
	std::vector<std::string> my_locked = locker::get_locked(); //returns the names of all files locked by current process
	locker::clear();                                           //unlocks all locked files (do not call this if some lockfile is open)
	
	std::string my_data = locker::xread("a.txt");              //exclusive-reads a file and returns its content as a string
	locker::xwrite("a.txt", my_data);                          //exclusive-writes data to a file (type of data must be insertable to std::ofstream)
	locker::xwrite("a.txt", "value", ':', 42);                 //exclusive-writes multiple data to a file

*Copyright 2020 Jean Diogo ([Jango](mailto:jeandiogo@gmail.com))*
