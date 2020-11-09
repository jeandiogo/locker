////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Locker (C++ Library)
// Copyright (C) 2020 Jean Diogo (Jango) <jeandiogo@gmail.com>
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
// Locker is a C++20 library for Linux systems, providing functions to lock files so they can be accessed exclusively or used as inter-process mutexes.
// The locker provides process-safety but not thread-safety. Once a process has acquired the lock, its threads and future forks will not be stopped by it.
// If the lockfile does not exist it will be created, but an exception will be thrown if the lockfile is not a regular file or if its directory is not authorized for writing.
// When compiling with g++ use the flag "-std=c++20", available in GCC 10 or later.
// 
// Usage:
// 
// #include "locker.hpp"
// 
// locker::lock_guard_t my_lock = locker::lock_guard("a.lock"); //locks a file and automatically unlocks it before leaving current scope
// 
// std::string       my_data = locker::xread("a.txt");          //exclusively reads a file (throws if file does not exist) and returns its content as a string (trailing newlines are removed)
// std::vector<char> my_data = locker::xread<char>("a.txt");    //same, but does not remove trailing newlines and return content as a vector of user specified type
// std::vector<int>  my_data = locker::xread<int>("a.txt");     //note that in these cases trailing bytes will be ignored if the size of the file is not a multiple of the the size of the chosen type
// std::vector<long> my_data = locker::xread<long>("a.txt");    //also note that an eventual traling newline may be included if it turns the file size into a multiple of the type size
// locker::xread("a.txt", my_container);                        //opens input file and calls operator ">>" once from the filestream to the container passed as argument
// 
// locker::xwrite("a.txt", my_data);                            //exclusively writes formatted data to a file (data type must be insertable to std::fstream via a single call to operator "<<")
// locker::xwrite("a.txt", "value", ':', 42);                   //exclusively writes multiple data to file
// locker::xwrite<true>("a.txt", "order", ':', 66);             //use first template argument to append data instead of overwrite
// locker::xwrite<false, true>("a.txt", "foobar");              //use second template argument to write a trailing newline
// 
// locker::xflush("a.txt", my_vector);                          //exclusively writes binary data to a file (data must be an std::vector of any type)
// locker::xflush("a.txt", my_span);                            //same as above, but with an std::span instead of a vector
// locker::xflush<true>("a.txt", my_vector);                    //use template argument to append data instead of overwrite
// locker::xflush("a.txt", my_data_pointer, my_data_size);      //one can also send a raw void pointer to the data to be written and its length in bytes
// 
// locker::memory_map_t my_map       = locker::xmap("a.txt");       //exclusively maps a file to memory and returns a container that behaves like an array of unsigned chars (throws if file is does not exist or is not regular)
// locker::memory_map_t my_map       = locker::xmap<char>("a.txt"); //the type underlying the array can be chosen at instantiation via template argument
// locker::memory_map_t my_map       = locker::xmap<int>("a.txt");  //note that trailing bytes will be ignored if the size of the file is not a multiple of the size of the chosen type
// unsigned char        my_var       = my_map.at(N);                //gets the N-th element, throws if N is out of range
// unsigned char        my_var       = my_map[N];                   //same as above, but does not check range
//                      my_map.at(N) = V;                           //assigns the value V to the N-th element, throws if N is out of range
//                      my_map[N]    = V;                           //same as above, but does not check range
// unsigned char *      my_data      = my_map.get_data();           //gets a raw pointer to file's data, whose underlying type is the one designated at instantiation (default is unsigned char)
// unsigned char *      my_data      = my_map.data();               //same as above, for STL compatibility
// std::size_t          my_size      = my_map.get_size();           //gets data size (which is equals to size of file divided by the size of the type) 
// std::size_t          my_size      = my_map.size();               //same as above, for STL compatibility
// bool                 is_empty     = my_map.is_empty();           //returns true if map is ampty
// bool                 is_empty     = my_map.empty();              //same as above, for STL compatibility
// bool                 success      = my_map.flush();              //flushes data to file (unnecessary, since current process will be the only one accessing the file, and it will flush at destruction)
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <map>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

class locker
{
	class [[nodiscard]] lock_guard_t
	{
		std::pair<ino_t, dev_t> id;
		
		public:
		
		lock_guard_t(lock_guard_t const &) = delete;
		lock_guard_t(lock_guard_t &&) = delete;
		auto & operator=(lock_guard_t const &) = delete;
		auto & operator=(lock_guard_t &&) = delete;
		auto operator&() = delete;
		
		lock_guard_t(std::string const & filename)
		{
			id = lock(filename).first;
		}
		
		~lock_guard_t()
		{
			unlock(id);
		}
	};
	
	template <typename data_t = unsigned char>
	class [[nodiscard]] memory_map_t
	{
		std::pair<ino_t, dev_t>   id         = {0, 0};
		int                       descriptor = -1;
		std::size_t               data_size  =  0;
		data_t                  * data_ptr   = nullptr;
		std::string               filename   = "";
		
		public:
		
		memory_map_t(memory_map_t &) = delete;
		memory_map_t(memory_map_t &&) = delete;
		auto & operator=(memory_map_t const &) = delete;
		auto & operator=(memory_map_t &&) = delete;
		auto operator&() = delete;
		
		memory_map_t(std::string const & f) : filename(f)
		{
			if(filename.empty() or !std::filesystem::exists(filename) or !std::filesystem::is_regular_file(std::filesystem::status(filename)))
			{
				throw std::runtime_error("\"" + filename + "\" is not a regular file");
			}
			auto const guard = std::scoped_lock<std::mutex>(get_singleton().lockfiles_mutex);
			auto const lockfile = lock(filename);
			id = lockfile.first;
			descriptor = lockfile.second.first;
			try
			{
				struct stat file_status;
				if(fstat(descriptor, &file_status) < 0)
				{
					throw std::runtime_error("could not get size of \"" + filename + "\"");
				}
				data_size = static_cast<std::size_t>(file_status.st_size / static_cast<off_t>(sizeof(data_t)));
				data_ptr = static_cast<data_t *>(mmap(nullptr, data_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, descriptor, 0));
				if(data_ptr == MAP_FAILED)
				{
					throw std::runtime_error("could not map file \"" + filename + "\" to memory");
				}
			}
			catch(...)
			{
				unlock(id);
				throw;
			}
		}
		
		~memory_map_t()
		{
			msync(data_ptr, data_size, MS_SYNC);
			munmap(data_ptr, data_size);
			unlock(id);
		}
		
		auto & operator[](std::size_t const index)
		{
			return data_ptr[index];
		}
		
		auto const & operator[](std::size_t const index) const
		{
			return data_ptr[index];
		}
		
		auto & at(std::size_t const index)
		{
			if(index >= data_size)
			{
				throw std::runtime_error("index " + std::to_string(index) + " is out of the range");
			}
			return data_ptr[index];
		}
		
		auto const & at(std::size_t const index) const
		{
			if(index >= data_size)
			{
				throw std::runtime_error("index " + std::to_string(index) + " is out of the range");
			}
			return data_ptr[index];
		}
		
		auto get_data() const
		{
			return data_ptr;
		}
		
		auto data() const
		{
			return data_ptr;
		}
		
		auto get_size() const
		{
			return data_size;
		}
		
		auto size() const
		{
			return data_size;
		}
		
		auto is_empty() const
		{
			return (data_size == 0);
		}
		
		auto empty() const
		{
			return (data_size == 0);
		}
		
		auto flush()
		{
			return (msync(data_ptr, data_size, MS_SYNC) >= 0);
		}
	};
	
	std::mutex lockfiles_mutex;
	std::map<std::pair<ino_t, dev_t>, std::pair<int, int>> lockfiles;
	
	static locker & get_singleton()
	{
		static auto singleton = locker();
		return singleton;
	}
	
	static inline std::pair<std::pair<ino_t, dev_t>, std::pair<int, int>> lock(std::string const & filename)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().lockfiles_mutex);
		while(true)
		{
			mode_t mask = umask(0);
			int descriptor = open(filename.c_str(), O_RDWR | O_CREAT, 0666);
			umask(mask);
			if(descriptor < 0)
			{
				throw std::runtime_error("could not open file \"" + filename + "\"");
			}
			try
			{
				struct stat status;
				if(fstat(descriptor, &status) < 0)
				{
					throw std::runtime_error("could not get status of file \"" + filename + "\"");
				}
				auto id = std::make_pair(status.st_ino, status.st_dev);
				auto & lockfiles = get_singleton().lockfiles;
				if(lockfiles.contains(id))
				{
					close(descriptor);
					auto & lockfile = lockfiles.at(id);
					++lockfile.second;
					return std::make_pair(id, lockfile);
				}
				if(flock(descriptor, LOCK_EX) < 0)
				{
					throw std::runtime_error("could not lock file \"" + filename + "\"");
				}
				struct stat new_status;
				if(stat(filename.c_str(), &new_status) >= 0 and new_status.st_nlink > 0 and new_status.st_ino == status.st_ino and new_status.st_dev == status.st_dev)
				{
					id = std::make_pair(status.st_ino, status.st_dev);
					auto const lockfile = std::make_pair(descriptor, 1);
					lockfiles.emplace(id, lockfile);
					return std::make_pair(id, lockfile);
				}
				close(descriptor);
			}
			catch(...)
			{
				close(descriptor);
				throw;
			}
		}
	}
	
	static inline void unlock(std::pair<ino_t, dev_t> const & id)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().lockfiles_mutex);
		auto & lockfiles = get_singleton().lockfiles;
		if(lockfiles.contains(id))
		{
			auto & lockfile = lockfiles.at(id);
			if((--lockfile.second <= 0) and ((fsync(lockfile.first) < 0) or (close(lockfile.first) < 0) or !lockfiles.erase(id)))
			{
				auto filename = std::string(256, '\0');
				auto const link = "/proc/self/fd/" + std::to_string(lockfile.first);
				if(readlink(link.c_str(), &filename[0], 256 - 1) < 0)
				{
					filename.clear();
				}
				throw std::runtime_error("could not unlock file \"" + filename + "\"");
			}
		}
	}
		
	locker() = default;
	
	public:
	
	~locker()
	{
		auto const guard = std::scoped_lock<std::mutex>(lockfiles_mutex);
		for(auto & lockfile : lockfiles)
		{
			close(lockfile.second.first);
		}
		lockfiles.clear();
	}
	
	locker(locker const &) = delete;
	locker(locker &&) = delete;
	auto & operator=(locker const &) = delete;
	auto & operator=(locker &&) = delete;
	
	template <typename ... TS>
	static auto lock_guard(std::string const & filename)
	{
		return lock_guard_t(filename);
	}
	
	template <typename data_t = unsigned char>
	static auto xmap(std::string const & filename)
	{
		return memory_map_t<data_t>(filename);
	}
	
	static auto xread(std::string const & filename)
	{
		if(!std::filesystem::exists(filename))
		{
			throw std::runtime_error("file \"" + filename + "\" does not exist");
		}
		auto const guard = lock_guard(filename);
		auto input = std::fstream(filename, std::fstream::in | std::fstream::ate);
		if(!input.good())
		{
			throw std::runtime_error("could not open file \"" + filename + "\" for input");
		}
		auto data = std::string(static_cast<std::size_t>(input.tellg()), '\n');
		input.seekg(0);
		input.read(data.data(), static_cast<long>(data.size()));
		while(data.size() and data.back() == '\n')
		{
			data.pop_back();
		}
		return data;
	}
	
	template <typename T>
	static auto xread(std::string const & filename)
	{
		if(!std::filesystem::exists(filename))
		{
			throw std::runtime_error("file \"" + filename + "\" does not exist");
		}
		auto const guard = lock_guard(filename);
		std::vector<T> data;
		auto input = std::fstream(filename, std::fstream::in | std::fstream::ate | std::fstream::binary);
		if(!input.good())
		{
			throw std::runtime_error("could not open file \"" + filename + "\" for binary input");
		}
		data.resize(static_cast<std::size_t>(input.tellg()) / sizeof(T));
		input.seekg(0);
		input.read(reinterpret_cast<char *>(data.data()), static_cast<long>(data.size() * sizeof(T)));
		return data;
	}
	
	template <typename T>
	static auto xread(std::string const & filename, T & container)
	{
		if(!std::filesystem::exists(filename))
		{
			throw std::runtime_error("file \"" + filename + "\" does not exist");
		}
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
	
	static bool xremove(std::string const & filename)
	{
		auto const guard = lock_guard(filename);
		if(!std::filesystem::remove(filename))
		{
			throw std::runtime_error("could not remove \"" + filename + "\"");
		}
	}
};
