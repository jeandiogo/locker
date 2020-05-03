# Locker

Locker is a header-only C++20 class with static member functions to lock files in Linux systems, so they can be used as inter-process mutexes. **Be aware that locking a file does not prevent other processes to modify the locked file or what it is protecting. The locking policy is only valid between programs using this library.** All methods will throw an exception if an empty filename is given or if the program does not have permission to modify the locked file or the directory the locked file is stored. If the file to be locked does not exist, it will be created. If you want to read or write to a locked file, you still have to open it using the input/output method you preffer, and it is your responsability to handle input/ouput data races among threads inside your process. It may be a good practice to create a separate lockfile to each file you intend to use (e.g. to open "a.txt" with exclusivity, lock the file "a.txt.lock"). If you opened a locked file, close it before unlocking. Do not forget to unlock every file you have manually locked, and prefer to use the lock guard, which will automatically unlock the file before leaving current scope.

*To compile with GCC, use the flag "-std=c++2a".*

## Usage:

    #include "locker.hpp"

    bool success = locker::try_lock("a.lock");   //tries to lock a file once, returns immediately
    locker::lock("a.lock");                      //keep trying to lock a file, only returns when file is locked
    locker::unlock("a.lock");                    //unlocks a file if it is locked
    auto my_lock = locker::lock_guard("a.lock"); //locks a file and automatically unlocks it before leaving current scope
