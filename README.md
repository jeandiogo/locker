# Locker

Locker is a header-only dependency-free C++20 class with static member functions to lock files in Linux systems, so they can be used as inter-process mutexes. **Be aware that locking a file does not prevent other processes to modify the locked file or what it is protecting. The locking policy is only valid between programs using this library.** All methods will throw an exception if an empty filename is given or if the program does not have permission to modify the locked file or the directory the locked file is stored. If the file to be locked does not exist, it will be created. If you want to read or write to a locked file, you still have to open it using the input/output method you preffer, and it is your responsability to handle input/ouput data races among threads inside your process. It may be a good practice to create a separate lockfile to each file you intend to use (e.g. to open "a.txt" with exclusivity, lock the file "a.txt.lock"). If you opened a locked file, close it before unlocking. Do not forget to unlock every file you have manually locked, and prefer to use the lock guard, which will automatically unlock the file before leaving current scope.

*To compile with GCC, use the flag "-std=c++2a".*

## Usage:

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
