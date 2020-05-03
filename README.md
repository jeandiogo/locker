# Locker

Locker is a header-only C++20 class with static member functions to lock files in Linux systems, so they can be used as inter-process mutexes. **Be aware that locking a file does not prevent other programs from reading or writing to it.** The locking policy works only among programs using this library. All methods will throw an exception if an empty filename is given or if the program does not have permission to write to the file or to the directory the file is stored. If the file to be locked does not exist, it will be created. **After locking a file, you still need to open it using the input/output method you preffer.** It is a good practice to create a separate lockfile for each file you intend to use (e.g. if you want to open the file "a.txt" exclusively, create a lockfile called "a.txt.lock" and use it as a mutex). To compile with GCC, use the flag *-std=c++2a*.

Usage:

    #include "locker.hpp"

    bool success = locker::try_lock("a.lock");               //tries to lock a file, returns immediately
    bool success = locker::try_lock("a.lock", "b.lock");     //tries to lock multiple files, returns immediately
    bool success = locker::try_lock({"a.lock", "b.lock"});   //arguments can also be sent as a list or a vector of filenames

    locker::lock("a.lock");                                  //only returns when the file is locked
    locker::lock("a.lock", "b.lock");                        //only returns when all files are locked
    locker::lock({"a.lock", "b.lock", "c.lock"});            //arguments can also be sent as a list or a vector of filenames

    locker::unlock("a.lock");                                //unlocks a file (if locked)
    locker::unlock("a.block", "b.lock");                     //unlocks files in reverse order of function arguments (same as unlock<false>)
    locker::unlock<true>("a.block", "b.lock", "c.lock");     //set template argument to unlock in strict order of function arguments
    locker::unlock({"a.block", "b.lock", "c.lock"});         //arguments can also be sent as a list or a vector of filenames

    auto my_lock = locker::lock_guard("a.lock");             //locks a file and automatically unlocks it when leaving current scope
    auto my_lock = locker::lock_guard("a.lock", "b.lock");   //locks multiple files and automatically unlocks them when leaving current scope
    auto my_lock = locker::lock_guard({"a.lock", "b.lock"}); //arguments can also be sent as a list or a vector of filenames
