////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Locker (C++ library)
// Copyright 2020 Jean Kurpel Diogo (aka Jango) <jeandiogo@gmail.com>
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
// Locker is a header-only C++20 class with static member functions to lock files in Linux systems, so they can be accessed exclusively or as inter-process mutexes.	
// 
// [Disclaimers]
// 
// The locking policy is only valid between programs using this library, so locking a file does not prevent other processes from modifying it or what it protects.
// An exception will be throw if an empty filename is given, if a directory name is given, or if the program does not have permission to modify the file and its directory.
// All functions are variadic, accepting a single filename, multiple filenames, a list of filenames, or a vector of filenames.
// If the file to be locked does not exist, it will be created.
// If you want to read or write to a locked file, you still have to open it using "fstream" or "fopen" methods.
// Do not forget to unlock every file you have manually locked. And if you want to open a locked file, do not forget to close it before unlocking.
// It is also your responsability to handle input/ouput data races among threads inside your process.
// It may be a good practice to create a separate lockfile for each file you intend to use, and be consistent with the naming convention.
// Example: to open "a.txt" with exclusivity, lock the file, say, "a.txt.lock". This will prevent you from losing the lock in case you need to erase and recreate "a.txt".
// Last but not least, instead of manual locking/unlocking, prefer using the lock guard, which will automatically unlock the file before leaving the current scope.
// 
// [Usage]
// 
// (To compile with GCC, use the flag "-std=c++2a".)
// 
// #include "locker.hpp"
// 
// bool success = locker::try_lock("a.lock");   //tries to lock a file once, returns immediately
// locker::lock("a.lock");                      //keep trying to lock a file, only returns when file is locked
// locker::unlock("a.lock");                    //unlocks a file if it is locked
// auto my_lock = locker::lock_guard("a.lock"); //locks a file and automatically unlocks it before leaving current scope
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
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
			throw std::runtime_error("could not assert permission to write in \"" + filename + "\"");
		}
		auto permissions = std::filesystem::status(filename).permissions();
		auto const has_owner_permissions = file_info.st_uid == getuid() and (permissions & std::filesystem::perms::owner_read) != std::filesystem::perms::none and (permissions & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
		auto const has_group_permissions = file_info.st_gid == getgid() and (permissions & std::filesystem::perms::group_read) != std::filesystem::perms::none and (permissions & std::filesystem::perms::group_write) != std::filesystem::perms::none;
		auto const has_other_permissions = (permissions & std::filesystem::perms::others_read) != std::filesystem::perms::none and (permissions & std::filesystem::perms::others_write) != std::filesystem::perms::none;
		return has_owner_permissions or has_group_permissions or has_other_permissions;
	}
		
	locker() : descriptors_mutex(), descriptors()
	{
	}
	
	public:
	
	~locker()
	{
		for(auto const & descriptor : descriptors)
		{
			close(descriptor.second);
		}
	}
	
	locker(locker const &) = delete;
	locker(locker &&) = delete;
	auto & operator=(locker) = delete;
	
	static auto try_lock(std::string const & filename)
	{
		if(filename.empty() or filename.back() == '/')
		{
			throw std::runtime_error("lockfile's name must not be empty");
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
			throw std::runtime_error("does not have permission to lock file \"" + filename + "\"");
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
	
	static void unlock(std::string const & filename)
	{
		if(filename.empty() or filename.back() == '/')
		{
			return;
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
};
