////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Locker (C++ Library)
// Copyright (C) 2020 Jean "Jango" Diogo <jeandiogo@gmail.com>
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
// locker.hpp
// 
// A class with static functions to lock files in Linux systems, so they can be accessed exclusively or used as inter-process mutexes.
// The locker provides process-safety but not thread-safety. Once a process has acquired the lock, its threads and future forks will not be stopped by it.
// If the lockfile does not exist at lock, it will be created. If the lockfile is empty during unlock, it will be erased.
// An exception will be thrown if the given filename refers to a file which existis but is not regular, or if its directory is not authorized for writing.
// When compiling with g++ use the flag "-std=c++20" (available in GCC 10 or later).
// 
// Usage:
// 
// #include "locker.hpp"
// 
// locker::lock_guard_t my_lock = locker::lock_guard("a.lock");              //locks a file and automatically unlocks it before leaving current scope (an empty lockfile will be created if it does not exist)
// locker::lock_guard_t my_lock = locker::lock_guard<true>("a.lock");        //use first template argument to make it "non-blocking" (i.e. will throw instead of wait if file is already locked)
// locker::lock_guard_t my_lock = locker::lock_guard<false, true>("a.lock"); //use second template argument to not erase empty lockfiles (by default empty lockfiles are erased at destruction)
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef LOCKER_HPP
#define LOCKER_HPP

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef PATH_MAX
	#define PATH_MAX 4096
#endif

class locker
{
	struct key_t
	{
		::ino_t inode = 0;
		::dev_t device = 0;
		
		key_t(::ino_t _inode, ::dev_t _device) : inode(_inode), device(_device)
		{
		}
		
		~key_t()
		{
			inode = 0;
			device = 0;
		}
		
		key_t() = default;
		key_t(key_t const & other) = default;
		key_t(key_t && other) = default;
		key_t & operator=(key_t const & other) = default;
		key_t & operator=(key_t && other) = default;
		
		friend auto operator==(key_t const & lhs, key_t const & rhs)
		{
			return lhs.inode == rhs.inode and lhs.device == rhs.device;
		}
		
		friend auto operator<(key_t const & lhs, key_t const & rhs)
		{
			return lhs.inode < rhs.inode or (lhs.inode == rhs.inode and lhs.device < rhs.device);
		}
	};
	
	struct value_t
	{
		int descriptor = -1;
		int num_locks = 0;
		::pid_t pid = -1;
		
		value_t() = default;
		value_t(value_t const & other) = default;
		value_t(value_t && other) = default;
		value_t & operator=(value_t const & other) = default;
		value_t & operator=(value_t && other) = default;
		
		value_t(int _descriptor, int _num_locks, ::pid_t _pid) : descriptor(_descriptor), num_locks(_num_locks), pid(_pid)
		{
		}
		
		~value_t()
		{
			descriptor = -1;
			num_locks = 0;
			pid = -1;
		}
	};
	
	std::mutex mtx;
	std::map<key_t, value_t> lockfiles;
	
	static auto & get_singleton()
	{
		static auto singleton = locker();
		return singleton;
	}
	
	template <bool should_not_block>
	static inline auto lock(std::string const & filename)
	{
		auto & singleton = get_singleton();
		auto const guard = std::scoped_lock<std::mutex>(singleton.mtx);
		while(true)
		{
			::mode_t mask = ::umask(0);
			int descriptor = ::open(filename.c_str(), O_RDWR | O_CREAT, 0666);
			::umask(mask);
			if(descriptor < 0)
			{
				throw std::runtime_error("could not open file \"" + filename + "\" for lock");
			}
			try
			{
				struct ::stat status;
				if(::fstat(descriptor, &status) < 0)
				{
					throw std::runtime_error("could not get status of file \"" + filename + "\"");
				}
				auto id = key_t(status.st_ino, status.st_dev);
				auto const pid = ::getpid();
				if(singleton.lockfiles.contains(id))
				{
					if(singleton.lockfiles.at(id).pid == pid)
					{
						::close(descriptor);
						auto & lockfile = singleton.lockfiles.at(id);
						++lockfile.num_locks;
						return std::make_pair(id, lockfile);
					}
					else
					{
						singleton.lockfiles.erase(id);
					}
				}
				auto flag = LOCK_EX;
				if constexpr(should_not_block)
				{
					flag |= LOCK_NB;
				}
				if(::flock(descriptor, flag) < 0)
				{
					throw std::runtime_error("could not lock file \"" + filename + "\"");
				}
				struct ::stat new_status;
				if(::stat(filename.c_str(), &new_status) >= 0 and new_status.st_nlink > 0 and new_status.st_ino == status.st_ino and new_status.st_dev == status.st_dev)
				{
					id = key_t(status.st_ino, status.st_dev);
					auto const lockfile = value_t(descriptor, 1, pid);
					singleton.lockfiles.emplace(id, lockfile);
					return std::make_pair(id, lockfile);
				}
				::close(descriptor);
			}
			catch(...)
			{
				::close(descriptor);
				throw;
			}
		}
	}
	
	template <bool should_keep_trace>
	static inline auto release(int const descriptor)
	{
		struct ::stat descriptor_stat;
		if(::fstat(descriptor, &descriptor_stat) < 0)
		{
			throw std::runtime_error("could not fstat descriptor \"" + std::to_string(descriptor) + "\"");
		}
		auto const link = "/proc/self/fd/" + std::to_string(descriptor);
		auto filename = std::string(static_cast<std::size_t>(PATH_MAX) + 1, '\0');
		auto size = ::readlink(link.c_str(), &filename[0], filename.size() - 1);
		if(size < 0 or size > PATH_MAX)
		{
			throw std::runtime_error("could not readlink descriptor \"" + std::to_string(descriptor) + "\"");
		}
		filename = filename.c_str();
		if(descriptor_stat.st_nlink > 0)
		{
			if constexpr(!should_keep_trace)
			{
				struct ::stat filelink_stat;
				if(::stat(filename.c_str(), &filelink_stat) < 0)
				{
					throw std::runtime_error("could not stat filename \"" + filename + "\"");
				}
				if(descriptor_stat.st_dev != filelink_stat.st_dev or descriptor_stat.st_ino != filelink_stat.st_ino)
				{
					throw std::runtime_error("could not match file descriptor \"" + std::to_string(descriptor) + "\" with filename \"" + filename + "\"");
				}
			}	
			if(::fsync(descriptor) < 0)
			{
				throw std::runtime_error("could not fsync file \"" + filename + "\"");
			}
			if constexpr(!should_keep_trace)
			{	
				size = ::lseek(descriptor, 0, SEEK_END);
				if(size < 0)
				{
					throw std::runtime_error("could not lseek file \"" + filename + "\"");
				}
				if(size == 0 and ::unlink(filename.c_str()) < 0)
				{
					throw std::runtime_error("could not unlink file \"" + filename + "\"");
				}
			}
		}
		if(::close(descriptor) < 0)
		{
			throw std::runtime_error("could not close file \"" + filename + "\"");
		}
		return filename;
	}
	
	template <bool should_keep_trace>
	static inline auto unlock(key_t const & id)
	{
		auto & singleton = get_singleton();
		auto const guard = std::scoped_lock<std::mutex>(singleton.mtx);
		if(singleton.lockfiles.contains(id))
		{
			auto & lockfile = singleton.lockfiles.at(id);
			if(--lockfile.num_locks <= 0)
			{
				auto const filename = release<should_keep_trace>(lockfile.descriptor);
				if(!singleton.lockfiles.erase(id))
				{
					throw std::runtime_error("could not erase file \"" + filename + "\" from locker");
				}
			}
		}
	}
	
	~locker()
	{
		auto const guard = std::scoped_lock<std::mutex>(mtx);
		for(auto && [key, value] : lockfiles)
		{
			try
			{
				release<true>(value.descriptor);
			}
			catch(...)
			{
			}
		}
		lockfiles.clear();
	}
	
	locker() = default;
	
	public:
	
	locker(locker const &) = delete;
	locker(locker &&) = delete;
	locker & operator=(locker const &) = delete;
	locker & operator=(locker &&) = delete;
	
	template <bool should_not_block, bool should_keep_trace>
	class [[nodiscard]] lock_guard_t
	{
		key_t id;
		
		public:
		
		lock_guard_t(lock_guard_t const &) = delete;
		lock_guard_t(lock_guard_t &&) = delete;
		lock_guard_t & operator=(lock_guard_t const &) = delete;
		lock_guard_t & operator=(lock_guard_t &&) = delete;
		lock_guard_t * operator&() = delete;
		
		lock_guard_t(std::string const & filename)
		{
			id = lock<should_not_block>(filename).first;
		}
		
		~lock_guard_t()
		{
			unlock<should_keep_trace>(id);
		}
	};
	
	template <bool should_not_block = false, bool should_keep_trace = false>
	static auto lock_guard(std::string const & filename)
	{
		return lock_guard_t<should_not_block, should_keep_trace>(filename);
	}
};

#endif
