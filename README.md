# Locker

Locker is a header-only C++20 class with static member functions to lock files in Linux systems, so they can be accessed exclusively or used as inter-process mutexes. **The locking policy is valid only between programs using this library, so locking a file does not prevent other processes from accessing it or what it protects.** Also, the lock is process-safe but not thread-safe, so once a process acquired the lock, its threads will not be stopped by the lock. The same is true for forks occured after the lock, so avoid forking a program while it has some file locked.

An exception will be throw if an empty filename is given, if a directory name is given, or if the program does not have permission to modify the file and its directory. If the file to be locked does not exist, it will be created. All locking and unlocking functions are variadic, accepting a single filename, multiple filenames, a list of filenames, or a vector of filenames. If you have manually locked a file, do not forget to unlock it. Nevertheless, instead of manually locking/unlocking a file, prefer using the lock guard, which will automatically unlock the file before leaving its scope of declaration.

Lock/unlock operations are independent from open/close operations. If you want to open a lockfile, you need to use "fstream" or "fopen" methods, and close the file before uncloking it. It is your responsability to handle race conditions among threads that have opened a file locked by the process that spawn them. If possible, instead of manually opening a lockfile, use the functions this library provides to perform exclusive read and exclusive write (which are both process-safe and thread-safe).

Finally, it may be a good practice to create a separate lockfile for each file you intend to use (and be consistent with your lockfile's names throughout your programs). This will prevent losing the lock in case you need to erase and recreate the file. For example: to exclusively open "a.txt", lock the file "a.txt.lock".

*When compiling with g++ use the flag "-std=c++2a" (available in GCC 7.0 or later).*

## Usage:

    #include "locker.hpp"

    bool success = locker::try_lock("a.lock");               //tries to lock a file once, returns immediately
    bool success = locker::try_lock("a.lock", "b.lock");     //tries to lock multiple files once, returns immediately
    bool success = locker::try_lock({"a.lock", "b.lock"});   //same as above

    locker::lock("a.lock");                                  //keeps trying to lock a file, only returns when file is locked
    locker::lock("a.lock", "b.lock");                        //keeps trying to lock multiple files, only returns when files are locked
    locker::lock({"a.lock", "b.lock"});                      //same as above

    locker::unlock("a.lock");                                //unlocks a file if it is locked
    locker::unlock("a.lock", "b.lock");                      //unlocks multiple files if they are locked
    locker::unlock({"a.lock", "b.lock"});                    //same as above

    auto my_lock = locker::lock_guard("a.lock");             //locks a file and automatically unlocks it before leaving current scope
    auto my_lock = locker::lock_guard("a.lock", "b.lock");   //locks multiple files and automatically unlocks them before leaving current scope
    auto my_lock = locker::lock_guard({"a.lock", "b.lock"}); //same as above

    std::string my_data = locker::xread("a.txt");            //performs an exclusive read of a file and returns its content as a string
    
    locker::xwrite("a.txt", my_data);                        //performs an exclusive write of an argument to a file
    locker::xwrite("a.txt", "value", ':', 42);               //performs an exclusive write of multiple arguments to a file
