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
// If the lockfile does not exist it will be created. If the lockfile is empty during unlock, it will be erased.
// An exception will be thrown if the given filename refers to a file which existis but is not regular, or if its directory is not authorized for writing.
// When compiling with g++ use the flag "-std=c++20" (available in GCC 10 or later).
// 
// Usage:
// 
// #include "locker.hpp"
// 
// locker::lock_guard_t my_lock = locker::lock_guard("a.lock");              //locks a file and automatically unlocks it before leaving current scope (creates lockfile if it does not exist)
// locker::lock_guard_t my_lock = locker::lock_guard<true>("a.lock");        //same, but does not delete the lockfile in case it is an empty file
// locker::lock_guard_t my_lock = locker::lock_guard<false, true>("a.lock"); //use second template argument to make it non-blocking (will throw if file is already locked)
// 
// std::string       my_data = locker::xread("a.txt");                       //exclusively reads a file and returns its content as a string (returns an empty string if file does not exist)
// std::string       my_data = locker::xread<true>("a.txt");                 //use template argument to remove trailing newlines ("\n" and "\r\n")
// std::vector<char> my_data = locker::xread<char>("a.txt");                 //use template typename to get file content as a vector of some specified type
// std::vector<int>  my_data = locker::xread<int>("a.txt");                  //note that in these cases trailing bytes will be ignored if the size of the file is not a multiple of the size of the chosen type
// std::vector<long> my_data = locker::xread<long>("a.txt");                 //also note that an eventual traling newline may be included if it turns the file size into a multiple of the type size
// locker::xread("a.txt", my_container);                                     //opens input file and calls operator ">>" once from the filestream to the container passed as argument
// 
// locker::xwrite("a.txt", my_data);                                         //exclusively writes formatted data to a file (data type must be insertable to std::fstream via a single call to operator "<<")
// locker::xwrite("a.txt", "value", ':', 42);                                //exclusively writes multiple data to file
// locker::xwrite<true>("a.txt", "order", ':', 66);                          //use first template argument to append data instead of overwrite
// locker::xwrite<false, true>("a.txt", "foobar");                           //use second template argument to write a trailing newline
// 
// locker::xflush("a.txt", my_vector);                                       //exclusively writes binary data from a std::vector to a file
// locker::xflush("a.txt", my_span);                                         //same as above, but with an std::span instead of a vector
// locker::xflush("a.txt", my_data_pointer, my_data_size);                   //you can also send a raw void pointer to the data, and its length in bytes
// locker::xflush<true>("a.txt", my_vector);                                 //use template argument to append data instead of overwrite
// 
// locker::xremove("filename");                                              //locks a file, then removes it
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <span>
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
		::ino_t inode  = 0;
		::dev_t device = 0;
		
		key_t(::ino_t _inode, ::dev_t _device) : inode(_inode), device(_device)
		{
		}
		
		key_t() = default;
		key_t(key_t const &) = default;
		key_t(key_t &&) = default;
		key_t & operator=(key_t const &) = default;
		key_t & operator=(key_t &&) = default;
		
		friend auto operator==(key_t const & x, key_t const & y)
		{
			return x.inode == y.inode and x.device == y.device;
		}
		
		friend auto operator<(key_t const & x, key_t const & y)
		{
			return x.inode < y.inode and x.device < y.device;
		}
	};
	
	struct value_t
	{
		int descriptor = -1;
		int num_locks  =  0;
		::pid_t pid    = -1;
		
		value_t(int _descriptor, int _num_locks, ::pid_t _pid) : descriptor(_descriptor), num_locks(_num_locks), pid(_pid)
		{
		}
		
		value_t() = default;
		value_t(value_t const &) = default;
		value_t(value_t &&) = default;
		value_t & operator=(value_t const &) = default;
		value_t & operator=(value_t &&) = default;
	};
	
	std::map<key_t, value_t> lockfiles;
	std::mutex lockfiles_mutex;
	
	static auto & get_singleton()
	{
		static auto singleton = locker();
		return singleton;
	}
	
	template <bool is_non_blocking = false>
	static inline std::pair<key_t, value_t> lock(std::string const & filename)
	{
		auto & singleton = get_singleton();
		auto const guard = std::scoped_lock<std::mutex>(singleton.lockfiles_mutex);
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
				if constexpr(is_non_blocking)
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
	
	template <bool should_keep_empty = false>
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
			if constexpr(should_keep_empty)
			{
				if(::fsync(descriptor) < 0)
				{
					throw std::runtime_error("could not fsync file \"" + filename + "\"");
				}
				if(::close(descriptor) < 0)
				{
					throw std::runtime_error("could not close file \"" + filename + "\"");
				}
			}
			else
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
				if(::fsync(descriptor) < 0)
				{
					throw std::runtime_error("could not fsync file \"" + filename + "\"");
				}
				size = ::lseek(descriptor, 0, SEEK_END);
				if(size < 0)
				{
					throw std::runtime_error("could not lseek file \"" + filename + "\"");
				}
				if(size == 0 and ::unlink(filename.c_str()) < 0)
				{
					throw std::runtime_error("could not unlink file \"" + filename + "\"");
				}
				if(::close(descriptor) < 0)
				{
					throw std::runtime_error("could not close file \"" + filename + "\"");
				}
			}
		}
		else
		{
			if(::close(descriptor) < 0)
			{
				throw std::runtime_error("could not close file \"" + filename + "\"");
			}
		}
		return filename;
	}
	
	template <bool should_keep_empty = false>
	static inline void unlock(key_t const & id)
	{
		auto & singleton = get_singleton();
		auto const guard = std::scoped_lock<std::mutex>(singleton.lockfiles_mutex);
		if(singleton.lockfiles.contains(id))
		{
			auto & lockfile = singleton.lockfiles.at(id);
			if(--lockfile.num_locks <= 0)
			{
				auto const filename = release<should_keep_empty>(lockfile.descriptor);
				if(!singleton.lockfiles.erase(id))
				{
					throw std::runtime_error("could not erase file \"" + filename + "\" from locker");
				}
			}
		}
	}
	
	~locker()
	{
		auto const guard = std::scoped_lock<std::mutex>(lockfiles_mutex);
		for(auto const & lockfile : lockfiles)
		{
			try
			{
				release<true>(lockfile.second.descriptor);
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
	auto & operator=(locker const &) = delete;
	auto & operator=(locker &&) = delete;
	
	template <bool should_keep_empty = false, bool is_non_blocking = false>
	static auto lock_guard(std::string const & filename)
	{
		return lock_guard_t<should_keep_empty, is_non_blocking>(filename);
	}
	
	template <bool should_strip_newlines = false>
	static auto xread(std::string const & filename)
	{
		auto const guard = lock_guard(filename);
		auto input = std::fstream(filename, std::fstream::in | std::fstream::ate);
		if(!input.good())
		{
			throw std::runtime_error("could not open file \"" + filename + "\" for input");
		}
		auto data = std::string(static_cast<std::size_t>(input.tellg()), '\0');
		if(data.size())
		{
			input.seekg(0);
			input.read(data.data(), static_cast<long>(data.size()));
			if constexpr(should_strip_newlines)
			{
				while(data.size() and data.back() == '\n')
				{
					data.pop_back();
					if(data.size() and data.back() == '\r')
					{
						data.pop_back();
					}
				}
			}
		}
		return data;
	}
	
	template <typename T>
	static auto xread(std::string const & filename)
	{
		auto const guard = lock_guard(filename);
		std::vector<T> data;
		auto input = std::fstream(filename, std::fstream::in | std::fstream::ate | std::fstream::binary);
		if(!input.good())
		{
			throw std::runtime_error("could not open file \"" + filename + "\" for binary input");
		}
		data.resize(static_cast<std::size_t>(input.tellg()) / sizeof(T));
		if(data)
		{
			input.seekg(0);
			input.read(reinterpret_cast<char *>(data.data()), static_cast<long>(data.size() * sizeof(T)));
		}
		return data;
	}
	
	template <typename T>
	static auto xread(std::string const & filename, T & container)
	{
		auto const guard = lock_guard(filename);
		auto input = std::fstream(filename, std::fstream::in);
		if(!input.good())
		{
			throw std::runtime_error("could not open file \"" + filename + "\" for input");
		}
		input >> container;
	}
	
	template <bool should_append = false, bool should_add_newline = false, typename ... TS>
	static auto xwrite(std::string const & filename, TS && ... data)
	{
		auto const guard = lock_guard(filename);
		auto flag = std::fstream::out;
		if constexpr(should_append)
		{
			flag |= std::fstream::app;
		}
		auto output = std::fstream(filename, flag);
		if(!output.good())
		{
			throw std::runtime_error("could not open file \"" + filename + "\" for output");
		}
		if constexpr(should_add_newline)
		{
			(output << ... << std::forward<TS>(data)) << std::endl;
		}
		else
		{
			(output << ... << std::forward<TS>(data)) << std::flush;
		}
	}
	
	template <bool should_append = false, typename T>
	static auto xflush(std::string const & filename, std::vector<T> const & data)
	{
		auto const guard = lock_guard(filename);
		auto flag = std::fstream::out | std::fstream::binary;
		if constexpr(should_append)
		{
			flag |= std::fstream::app;
		}
		auto output = std::fstream(filename, flag);
		if(!output.good())
		{
			throw std::runtime_error("could not open file \"" + filename + "\" for binary output");
		}
		output.write(reinterpret_cast<char const *>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(T)));
		output.flush();
	}
	
	template <bool should_append = false, typename T>
	static auto xflush(std::string const & filename, std::span<T> const data)
	{
		auto const guard = lock_guard(filename);
		auto flag = std::fstream::out | std::fstream::binary;
		if constexpr(should_append)
		{
			flag |= std::fstream::app;
		}
		auto output = std::fstream(filename, flag);
		if(!output.good())
		{
			throw std::runtime_error("could not open file \"" + filename + "\" for binary output");
		}
		output.write(reinterpret_cast<char const *>(data.data()), static_cast<std::streamsize>(data.size() * sizeof(T)));
		output.flush();
	}
	
	template <bool should_append = false>
	static auto xflush(std::string const & filename, void * data, std::size_t const size)
	{
		auto const guard = lock_guard(filename);
		auto flag = std::fstream::out | std::fstream::binary;
		if constexpr(should_append)
		{
			flag |= std::fstream::app;
		}
		auto output = std::fstream(filename, flag);
		if(!output.good())
		{
			throw std::runtime_error("could not open file \"" + filename + "\" for binary output");
		}
		output.write(static_cast<char *>(data), static_cast<std::streamsize>(size));
		output.flush();
	}
	
	static auto xremove(std::string const & filename)
	{
		auto const guard = lock_guard(filename);
		if(!std::filesystem::remove(filename))
		{
			throw std::runtime_error("could not remove \"" + filename + "\"");
		}
	}
	
	template <bool should_keep_empty = false, bool is_non_blocking = false>
	class [[nodiscard]] lock_guard_t
	{
		key_t id;
		
		public:
		
		lock_guard_t(lock_guard_t const &) = delete;
		lock_guard_t(lock_guard_t &&) = delete;
		auto & operator=(lock_guard_t const &) = delete;
		auto & operator=(lock_guard_t &&) = delete;
		auto operator&() = delete;
		
		lock_guard_t(std::string const & filename)
		{
			id = lock<is_non_blocking>(filename).first;
		}
		
		~lock_guard_t()
		{
			unlock<should_keep_empty>(id);
		}
	};
};
