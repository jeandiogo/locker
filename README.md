# Locker

Locker is a header-only C++20 class with static member functions to lock files in Linux systems, so they can be accessed exclusively or used as inter-process mutexes. **The locking policy is only valid between programs using this library, so locking a file does not prevent other processes from modifying it or what it protects.** An exception will be throw if an empty filename is given, if a directory name is given, or if the program does not have permission to modify the file and its directory. All functions are variadic, accepting a single filename, multiple filenames, a list of filenames, or a vector of filenames. If a file to be locked does not exist, it will be created. If you want to read or write to a locked file, you still have to open it using "fstream" or "fopen" methods. Do not forget to unlock every file you have manually locked. And if you want to open a locked file, do not forget to close it before unlocking. It is also your responsability to handle input/ouput data races among threads inside your process. It may be a good practice to create a separate lockfile for each file you intend to use, and be consistent with the naming convention. Example: to open "a.txt" with exclusivity, lock the file, say, "a.txt.lock". This will prevent you from losing the lock in case you need to erase and recreate "a.txt". Last but not least, instead of manual locking/unlocking, prefer using the lock guard, which will automatically unlock the file before leaving the current scope.

*When compiling with g++ use the flag "-std=c++2a" (available in GCC 7.0 or later).*

## Usage:

    #include "locker.hpp"

    bool success = locker::try_lock("a.lock");   //tries to lock a file once, returns immediately
    locker::lock("a.lock");                      //keeps trying to lock a file, only returns when file is locked
    locker::unlock("a.lock");                    //unlocks a file if it is locked
    auto my_lock = locker::lock_guard("a.lock"); //locks a file and automatically unlocks it before leaving current scope
