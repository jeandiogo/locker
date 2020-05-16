////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Locker (C++ Library)
// Copyright 2020 Jean Diogo (Jango) <jeandiogo@gmail.com>
// 
// Licensed under the Apache License Version 2.0 (the "License").
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at <http://www.apache.org/licenses/LICENSE-2.0>.
// 
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Locker is a header-only C++20 class with static member functions to lock files in Linux systems, so they can be accessed exclusively or used as inter-process mutexes.	
// 
// [Notice]
// 
// The locking policy works only among programs using this library, so locking a file does not prevent other processes from opening it, but it ensures that only one program will get the lock at a time. Moreover, the locker does not provide thread-safety. Once a process has acquired the lock, neither its threads and future forks will be stopped by it, nor they will be able to mutually exclude each other by using the filelock. Therefore, avoid forking a program while it has some file locked, and use ordinary mutexes to synchronize its inner threads.
// 
// An exception will be throw if an empty filename is given, if a directory name is given, or if the program does not have permission to read from and write to the file and its directory. If the file to be locked does not exist, it will be created. All locking and unlocking functions are variadic, accepting a single filename, multiple filenames, a list of filenames, or a vector of filenames. If you have manually locked a file, do not forget to unlock it. Nevertheless, prefer using the lock guard, which will automatically unlock the file before leaving its scope of declaration.
// 
// Be aware that lock and unlock operations are independent from open and close operations. If you want to open a lockfile, you need to use file handlers like "fstream" or "fopen", and close the file before unlocking it. It is also your responsability to handle race conditions among threads that have opened a file locked by their parent. If you prefer, instead of manually locking and opening a file, use the functions this library provides to perform exclusive read, write or append, which are all process-safe but still not thread-safe.
// 
// Finally, a process will loose the lock if the lockfile is deleted. So it may be a good practice to create separate lockfiles for each file you intend to use (e.g. to exclusively open "a.txt", lock the file "a.txt.lock"). This will prevent you from losing the lock in case you need to erase and recreate the file without losing the lock to other processes. Do not forget to be consistent with the name of lockfiles throughout your programs.
// 
// [Usage]
// 
// (When compiling with g++ use the flag "-std=c++2a", available in GCC 7.0 or later.)
// 
// #include "locker.hpp"
// 
// bool success = locker::try_lock("a.lock");                         //tries to lock a file once, returns immediately
// bool success = locker::try_lock("a.lock", "b.lock");               //tries to lock multiple files once, returns immediately
// bool success = locker::try_lock({"a.lock", "b.lock"});             //same as above
// 
// locker::lock("a.lock");                                            //keeps trying to lock a file, only returns when file is locked
// locker::lock("a.lock", "b.lock");                                  //keeps trying to lock multiple files, only returns when files are locked
// locker::lock({"a.lock", "b.lock"});                                //same as above
// 
// locker::lock(1, "a.lock");                                         //keeps trying to lock in intervals of approximately 1 millisecond
// locker::lock<std::chrono::nanoseconds>(1000, "a.lock");            //use template argument to change the unit of measurement
// locker::lock(20, "a.lock", "b.lock");                              //also works for the variadic versions
// 
// locker::unlock("a.lock");                                          //unlocks a file if it is locked
// locker::unlock("a.lock", "b.lock");                                //unlocks multiple files (in reverse order) if they are locked
// locker::unlock({"a.lock", "b.lock"});                              //same as above
// 
// auto my_lock = locker::lock_guard("a.lock");                       //locks a file and automatically unlocks it before leaving current scope
// auto my_lock = locker::lock_guard("a.lock", "b.lock");             //locks multiple files and automatically unlocks them before leaving current scope
// auto my_lock = locker::lock_guard({"a.lock", "b.lock"});           //same as above
// 
// std::string my_data = locker::xread("a.txt");                      //exclusive-reads a file and returns its content as a string
// std::vector<unsigned char> my_data = locker::xread<true>("a.bin"); //same as above, but opens the file in binary mode
// 
// locker::xwrite("a.txt", my_data);                                  //exclusive-writes data to a file (data type must be insertable to std::fstream)
// locker::xwrite<true>("a.bin", my_data);                            //same as above, but opens the file in binary mode
// locker::xwrite("a.txt", "value", ':', 42);                         //exclusive-writes multiple data to a file
// 
// locker::xappend("a.txt", my_data);                                 //exclusive-appends data to a file (data type must be insertable to std::fstream)
// locker::xappend<true>("a.bin", my_data);                           //same as above, but opens the file in binary mode
// locker::xappend("a.txt", "value", ':', 42);                        //exclusive-appends multiple data to a file
// 
// bool success = locker::is_locked("a.lock");                        //asserts if a file is already locked by current process
// std::vector<std::string> my_locked = locker::get_locked();         //returns the names of all files locked by current process
// locker::clear();                                                   //unlocks all locked files (do not call this if some lockfile is open)
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

class locker
{
	class [[nodiscard]] lock_guard_t
	{
		std::vector<std::string> filenames;
		
		public:
		
		lock_guard_t(lock_guard_t const &) = delete;
		lock_guard_t(lock_guard_t &&) = delete;
		auto & operator=(lock_guard_t) = delete;
		auto operator&() = delete;
		
		lock_guard_t(std::vector<std::string> && fs) : filenames(fs)
		{
			lock(filenames);
		}
		
		template <typename ... TS>
		lock_guard_t(TS && ... fs) : filenames({std::forward<TS>(fs) ...})
		{
			lock(filenames);
		}
		
		~lock_guard_t()
		{
			unlock(filenames);
		}
	};
		
	std::mutex descriptors_mutex;
	std::map<std::string, int> descriptors;
	
	static auto & get_singleton()
	{
		static auto singleton = locker();
		return singleton;
	}
	
	static inline auto has_permission(std::string const & filename)
	{
		struct stat file_info;
		if(stat(filename.c_str(), &file_info) != 0)
		{
			throw std::runtime_error("could not assert permission to write in \"" + filename + "\": " + std::string(strerror(errno)));
		}
		auto permissions = std::filesystem::status(filename).permissions();
		auto const has_owner_permissions = file_info.st_uid == getuid() and (permissions & std::filesystem::perms::owner_read) != std::filesystem::perms::none and (permissions & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
		auto const has_group_permissions = file_info.st_gid == getgid() and (permissions & std::filesystem::perms::group_read) != std::filesystem::perms::none and (permissions & std::filesystem::perms::group_write) != std::filesystem::perms::none;
		auto const has_other_permissions = (permissions & std::filesystem::perms::others_read) != std::filesystem::perms::none and (permissions & std::filesystem::perms::others_write) != std::filesystem::perms::none;
		return has_owner_permissions or has_group_permissions or has_other_permissions;
	}
		
	locker()
	{
	}
	
	public:
	
	~locker()
	{
		clear();
	}
	
	locker(locker const &) = delete;
	locker(locker &&) = delete;
	auto & operator=(locker) = delete;
	
	static void clear()
	{
		auto const descriptors_guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		for(auto & descriptor : descriptors)
		{
			close(descriptor.second);
		}
		descriptors.clear();
	}
	
	static auto is_locked(std::string const & filename)
	{
		return get_singleton().descriptors.contains(filename);
	}
	
	static auto get_locked()
	{
		std::vector<std::string> locked;
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		locked.reserve(descriptors.size());
		for(auto const & descriptor : descriptors)
		{
			locked.emplace_back(descriptor.first);
		}
		return locked;
	}
	
	static auto try_lock(std::string const & filename)
	{
		if(filename.empty() or filename.back() == '/')
		{
			throw std::runtime_error("cannot lock \"\": filename must not be empty");
		}
		std::string path_to_file = "./";
		for(std::size_t i = filename.size() - 1; static_cast<long>(i) >= 0; --i)
		{
			if(filename[i] == '/')
			{
				path_to_file = std::string(filename, 0, i + 1);
				break;
			}
		}
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		if(descriptors.contains(filename))
		{
			return true;
		}
		auto const has_permission_to_path = (std::filesystem::exists(path_to_file) or std::filesystem::create_directories(path_to_file)) and has_permission(path_to_file);
		auto const has_permission_to_file = !std::filesystem::exists(filename) or (std::filesystem::is_regular_file(std::filesystem::status(filename)) and has_permission(filename));
		if(!has_permission_to_path or !has_permission_to_file)
		{
			throw std::runtime_error("cannot lock \"" + filename + "\": file must be regular and writeable, or non-existing");
		}
		mode_t mask = umask(0);
		int descriptor = open(filename.c_str(), O_RDWR | O_CREAT, 0666);
		umask(mask);
		if(descriptor < 0)
		{
			return false;
		}
		if(flock(descriptor, LOCK_EX | LOCK_NB) < 0)
		{
			close(descriptor);
			descriptor = -1;
			return false;
		}
		descriptors[filename] = descriptor;
		return true;
	}
	
	template <typename ... TS>
	static auto try_lock(std::string const & filename, TS && ... filenames)
	{
		if(!try_lock(filename))
		{
			return false;
		}
		if(!try_lock(std::forward<TS>(filenames) ...))
		{
			unlock(filename);
			return false;
		}
		return true;
	}
	
	static void try_lock(std::vector<std::string> const & filenames)
	{
		for(std::size_t i = 0; i < filenames.size(); ++i)
		{
			if(!try_lock(filenames[i]))
			{
				for(std::size_t j = i - 1; static_cast<long>(j) >= 0; --j)
				{
					unlock(filenames[j]);
				}
			}
		}
	}
	
	static void lock(std::string const & filename)
	{
		while(!try_lock(filename))
		{
		}
	}
	
	template <typename ... TS>
	static void lock(std::string const & filename, TS && ... filenames)
	{
		while(!try_lock(filename, std::forward<TS>(filenames) ...))
		{
		}
	}
	
	static void lock(std::vector<std::string> const & filenames)
	{
		for(auto it = filenames.begin(); it != filenames.end(); ++it)
		{
			lock(*it);
		}
	}
	
	template <typename T = std::chrono::milliseconds>
	static void lock(long timespan, std::string const & filename)
	{
		while(!try_lock(filename))
		{
			if(timespan)
			{
				std::this_thread::sleep_for(T(std::abs(timespan)));
			}
		}
	}
	
	template <typename T = std::chrono::milliseconds, typename ... TS>
	static void lock(long timespan, std::string const & filename, TS && ... filenames)
	{
		while(!try_lock(filename, std::forward<TS>(filenames) ...))
		{
			if(timespan)
			{
				std::this_thread::sleep_for(T(std::abs(timespan)));
			}
		}
	}
	
	template <typename T = std::chrono::milliseconds>
	static void lock(long timespan, std::vector<std::string> const & filenames)
	{
		for(auto it = filenames.begin(); it != filenames.end(); ++it)
		{
			lock(timespan, *it);
		}
	}
	
	static void unlock(std::string const & filename)
	{
		if(filename.empty() or filename.back() == '/')
		{
			throw std::runtime_error("cannot unlock \"\": filename must not be empty");
		}
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		if(descriptors.contains(filename))
		{
			close(descriptors.at(filename));
			descriptors.erase(filename);
		}
	}
	
	template <typename ... TS>
	static void unlock(std::string const & filename, TS && ... filenames)
	{
		unlock(std::forward<TS>(filenames) ...);
		unlock(filename);
	}
	
	static void unlock(std::vector<std::string> const & filenames)
	{
		for(auto it = filenames.rbegin(); it != filenames.rend(); ++it)
		{
			unlock(*it);
		}
	}
	
	template <typename ... TS>
	static auto lock_guard(TS && ... filenames)
	{
		return lock_guard_t(std::forward<TS>(filenames) ...);
	}
	
	static auto lock_guard(std::vector<std::string> const & filenames)
	{
		return lock_guard_t(filenames);
	}
	
	template <bool should_add_binary_flag = false>
	static auto xread(std::string const & filename)
	{
		auto should_lock = !is_locked(filename);
		if(should_lock)
		{
			lock(filename);
		}
		try
		{
			if constexpr(should_add_binary_flag)
			{
				auto input = std::fstream(filename, std::fstream::in | std::fstream::ate | std::fstream::binary);
				if(!input.good())
				{
					throw std::runtime_error("could not open file \"" + filename + "\" for input");
				}
				auto data = std::vector<unsigned char>(static_cast<std::size_t>(input.tellg()), '\0');
				input.seekg(0);
				input.read(reinterpret_cast<char *>(&data[0]), static_cast<long>(data.size()));
				while(data.size() and data.back() == '\n')
				{
					data.pop_back();
				}
				if(should_lock)
				{
					unlock(filename);
				}
				return data;
			}
			else
			{
				auto input = std::fstream(filename, std::fstream::in | std::fstream::ate);
				if(!input.good())
				{
					throw std::runtime_error("could not open file \"" + filename + "\" for input");
				}
				auto data = std::string(static_cast<std::size_t>(input.tellg()), '\0');
				input.seekg(0);
				input.read(&data[0], static_cast<long>(data.size()));
				while(data.size() and data.back() == '\n')
				{
					data.pop_back();
				}
				if(should_lock)
				{
					unlock(filename);
				}
				return data;
			}
		}
		catch(...)
		{
			if(should_lock)
			{
				unlock(filename);
			}
			throw;
		}
	}
	
	template <bool should_add_binary_flag = false, typename ... TS>
	static auto xwrite(std::string const & filename, TS && ... data)
	{
		auto should_lock = !is_locked(filename);
		if(should_lock)
		{
			lock(filename);
		}
		try
		{
			std::fstream output;
			if constexpr(should_add_binary_flag)
			{
				output.open(filename, std::fstream::out | std::fstream::binary);
			}
			else
			{
				output.open(filename, std::fstream::out);
			}
			if(!output.good())
			{
				throw std::runtime_error("could not open file \"" + filename + "\" for output");
			}
			(output << ... << std::forward<TS>(data)) << std::flush;
		}	
		catch(...)
		{
			if(should_lock)
			{
				unlock(filename);
			}
			throw;
		}
		if(should_lock)
		{
			unlock(filename);
		}
	}

	template <bool should_add_binary_flag = false, typename ... TS>
	static auto xappend(std::string const & filename, TS && ... data)
	{
		auto should_lock = !is_locked(filename);
		if(should_lock)
		{
			lock(filename);
		}
		try
		{
			std::fstream output;
			if constexpr(should_add_binary_flag)
			{
				output.open(filename, std::fstream::app | std::fstream::binary);
			}
			else
			{
				output.open(filename, std::fstream::app);
			}
			if(!output.good())
			{
				throw std::runtime_error("could not open file \"" + filename + "\" for append");
			}
			(output << ... << std::forward<TS>(data)) << std::flush;
		}	
		catch(...)
		{
			if(should_lock)
			{
				unlock(filename);
			}
			throw;
		}
		if(should_lock)
		{
			unlock(filename);
		}
	}
};
