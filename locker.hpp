////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Locker (C++ library)
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Copyright 2019-2020 Jean Kurpel Diogo (aka Jango) <jeandiogo@gmail.com>
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
// Locker is a header-only C++ class with static methods to lock files in Linux systems, so they can be used as inter-process mutexes.
// Locking a file does not prevent other programs from reading/writing to it. The locking policy is valid only for programs using this library.
// All methods will throw if an empty filename is given or if the program does not have permission to write to it and to its directory.
// If the file to be locked does not exist, it will be created.
// 
// Usage:
// 
// #include "locker.hpp"
// 
// bool success = locker::try_lock("a.lock");                     //tries to lock file
// bool success = locker::try_lock("a.lock", "b.lock");           //tries to lock multiple files
// bool success = locker::try_lock({"a.lock", "b.lock"});         //arguments can also be sent as a vector of filenames
// 
// locker::lock("a.lock");                                        //only returns when file is locked
// locker::lock("a.lock", "b.lock");                              //only returns when all files are locked
// locker::lock({"a.lock", "b.lock", "c.lock"});                  //same as above
// 
// locker::unlock("a.lock");                                      //unlocks file if locked
// locker::unlock("a.block", "b.lock");                           //unlocks files in the reverse order of the arguments (same as "unlock<false>")
// locker::unlock({"a.block", "b.lock", "c.lock"});               //same as above
// locker::unlock<true>("a.block", "b.lock", "c.lock");           //use template argument to unlock in the strict order of the arguments
// 
// auto my_lock_guard = locker::lock_guard("a.lock");             //locks a file and automatically unlocks it when leaving current scope
// auto my_lock_guard = locker::lock_guard("a.lock", "b.lock");   //locks multiple files and automatically unlocks them when leaving current scope
// auto my_lock_guard = locker::lock_guard({"a.lock", "b.lock"}); //same as above
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cstring>
#include <filesystem>
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
	std::map<std::string, int> file_descriptors;
	std::mutex file_descriptors_mutex;
	
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
	
	locker()
	{
	}
	
	public:
	
	~locker()
	{
		for(auto const & file_descriptor : file_descriptors)
		{
			close(file_descriptor.second);
		}
	}
	
	locker(locker const &) = delete;
	locker(locker &&) = delete;
	auto & operator=(locker) = delete;
	
	static auto unlock(std::string const & filename)
	{
		if(filename.empty())
		{
			return;
		}
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().file_descriptors_mutex);
		auto & fds = get_singleton().file_descriptors;
		if(fds.contains(filename))
		{
			close(fds.at(filename));
			fds.erase(filename);
		}
	}
	
	template <bool should_not_reverse = false, typename ... TS>
	static auto unlock(std::string const & filename, TS && ... filenames)
	{
		if constexpr(should_not_reverse)
		{
			unlock(filename);
			unlock(std::forward<TS>(filenames) ...);
		}
		else
		{
			unlock(std::forward<TS>(filenames) ...);
			unlock(filename);
		}
	}
	
	template <bool should_not_reverse = false>
	static auto unlock(std::vector<std::string> const & filenames)
	{
		if constexpr(should_not_reverse)
		{
			for(std::size_t i = 0; i < filenames.size(); ++i)
			{
				unlock(filenames[i]);
			}
		}
		else
		{
			for(long i = static_cast<long>(filenames.size() - 1); i >= 0; --i)
			{
				unlock(filenames[static_cast<std::size_t>(i)]);
			}
		}
	}
	
	static auto try_lock(std::string filename)
	{
		while(filename.size() and filename.back() == '/')
		{
			filename.pop_back();
		}
		if(filename.empty())
		{
			throw std::runtime_error("filename must not be an empty string");
		}
		std::string dirname = ".";
		for(long i = static_cast<long>(filename.size() - 1); i >= 0; --i)
		{
			if(filename[static_cast<std::size_t>(i)] == '/')
			{
				dirname = std::string(filename.begin(), filename.begin() + i);
				break;
			}
		}
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().file_descriptors_mutex);
		auto & file_descriptors = get_singleton().file_descriptors;
		if(file_descriptors.contains(filename))
		{
			return true;
		}
		auto directory_is_valid = (std::filesystem::exists(dirname) or std::filesystem::create_directories(dirname)) and has_permission(dirname);
		auto file_is_valid = !std::filesystem::exists(filename) or (std::filesystem::is_regular_file(std::filesystem::status(filename)) and has_permission(filename));
		if(!directory_is_valid or !file_is_valid)
		{
			throw std::runtime_error("does not have permission to lock file \"" + filename + "\"");
		}
		mode_t mask = umask(0);
		int file_descriptor = open(filename.c_str(), O_RDWR | O_CREAT, 0666);
		umask(mask);
		if(file_descriptor < 0)
		{
			return false;
		}
		if(flock(file_descriptor, LOCK_EX | LOCK_NB) < 0)
		{
			close(file_descriptor);
			file_descriptor = -1;
			return false;
		}
		file_descriptors[filename] = file_descriptor;
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
	
	static auto try_lock(std::vector<std::string> const & filenames)
	{
		for(long i = 0; i < static_cast<long>(filenames.size()); ++i)
		{
			if(!try_lock(filenames[static_cast<std::size_t>(i)]))
			{
				for(long j = i - 1; j >= 0; --j)
				{
					unlock(filenames[static_cast<std::size_t>(j)]);
				}
				return false;
			}
		}
		return true;
	}
	
	static auto lock(std::string const & filename)
	{
		while(!try_lock(filename))
		{
		}
	}
	
	template <typename ... TS>
	static auto lock(TS && ... filenames)
	{
		while(!try_lock(std::forward<TS>(filenames) ...))
		{
		}
	}
	
	static auto lock(std::vector<std::string> const & filenames)
	{
		while(!try_lock(filenames))
		{
		}
	}
	
	class lock_guard
	{
		std::vector<std::string> filenames;
		
		public:
		
		lock_guard(lock_guard const &) = delete;
		lock_guard(lock_guard &&) = delete;
		auto & operator=(lock_guard) = delete;
		auto operator&() = delete;
		
		template <typename ... TS>
		lock_guard(TS && ... f) : filenames({std::forward<TS>(f) ...})
		{
			lock(std::forward<TS>(f) ...);
		}
		
		~lock_guard()
		{
			for(auto it = filenames.rbegin(); it != filenames.rend(); ++it)
			{
				unlock(*it);
			}
		}
	};
};
