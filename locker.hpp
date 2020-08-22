////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Locker (C++ Library)
// Copyright (c) 2020 Jean Diogo (Jango) <jeandiogo@gmail.com>
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
// Locker is a single header C++20 class with static member functions to lock files in Linux systems, so they can be accessed exclusively or used for process synchronization.
// The locker provides process-safety but not thread-safety. Once a process has acquired the lock, its threads and future forks will not be stopped by it.
// If the lockfile does not exist it will be created, but an exception will be thrown if the lockfile is not a regular file or if its directory is not authorized for writing.
// The lock will be lost if the lockfile is deleted. To provide a minimal watch on that, the locker will throw an exception if the user tries to unlock a file that does not exist anymore.
// When compiling with g++ use the flag "-std=c++20", available in GCC 10 or later.
// 
// Usage:
// 
// #include "locker.hpp"
// 
// bool success = locker::try_lock("a.lock");                               //tries to lock a file once, returns immediately
// bool success = locker::try_lock("a.lock", "b.lock", "c.lock");           //tries to lock multiple files once, returns immediately
// bool success = locker::try_lock({"a.lock", "b.lock"});                   //tries to lock a initializer list or a vector of files once, returns immediately
// 
// locker::lock("a.lock");                                                  //keeps trying to lock a file, only returns when file is locked
// locker::lock("a.lock", "b.lock", "c.lock", "d.lock");                    //keeps trying to lock multiple files, only returns when all file are locked
// locker::lock({"a.lock", "b.lock"});                                      //keeps trying to lock a initializer list or a vector of files, only returns when all files are locked
// 
// locker::unlock("a.lock");                                                //unlocks a file if it is locked (throws if file does not exist)
// locker::unlock("a.lock", "b.lock");                                      //unlocks a multiple files (in reverse order) if they are locked
// locker::unlock({"a.lock", "b.lock", "c.lock"});                          //unlocks a initializer list or a vector of files (in reverse order) if they are locked
// 
// locker::lock_guard_t my_lock = locker::lock_guard("a.lock");             //locks a file and automatically unlocks it before leaving current scope
// locker::lock_guard_t my_lock = locker::lock_guard("a.lock", "b.lock");   //locks multiple files and automatically unlocks them before leaving current scope
// locker::lock_guard_t my_lock = locker::lock_guard({"a.lock", "b.lock"}); //locks a initializer list or a vector of files and automatically unlocks them before leaving current scope
// 
// std::string my_data = locker::xread("a.txt");                            //exclusively reads a file (throws if file does not exist) and returns its content as a string (trailing newlines are removed)
// std::vector<char> my_data = locker::xread<char>("a.txt");                //same, but does not remove trailing newlines and return content as a vector of user specified type (must be an integral type)
// std::vector<int> my_data = locker::xread<int>("a.txt");                  //note that trailing bytes will be ignored if the file size is not a multiple of the chosen type size
// std::vector<long> my_data = locker::xread<long>("a.txt");                //also note that traling newlines may be included if they turn the file size into a multiple of the type size
// 
// locker::xwrite("a.txt", my_data);                                        //exclusively writes formatted data to a file (data type must be insertable to std::fstream)
// locker::xwrite("a.txt", "value", ':', 42);                               //exclusively writes multiple data to a file
// locker::xwrite<true>("a.txt", "order", ':', 66);                         //use template argument to append data instead of overwrite
// 
// locker::xflush("a.txt", my_vector);                                      //exclusively writes binary data to a file (data must be an std::vector of any integral type)
// locker::xflush<true>("a.txt", my_vector);                                //use template argument to append data instead of overwrite
// locker::xflush("a.txt", my_data_pointer, my_data_size);                  //one can also send a raw void pointer and the length in bytes of the data to be written
// 
// locker::memory_map_t my_map = locker::xmap("a.txt");                     //exclusively maps a file to memory and returns a container that behaves like an array, throws if file does not exist or is not a regular file
// locker::memory_map_t my_map = locker::xmap<char>("a.txt");               //the type underlying the array can be designated at instantiation via template argument (must be an integral type), default is unsigned char
// locker::memory_map_t my_map = locker::xmap<int>("a.txt");                //note that trailing bytes will be ignored if size of file is not a multiple of the size of the designated type
// unsigned char my_var = my_map.at(N);                                     //gets the N-th memory position of the file based on the size of designated type, throws if N excedess file's range
// unsigned char my_var = my_map[N];                                        //same, but does not check range
// my_map.at(N) = M;                                                        //assigns the value M to the N-th position, throws if N excedess file's range
// my_map[N] = M;                                                           //same, but does not check range
// std::size_t my_size = my_map.get_size();                                 //gets the size of the array (which is size of file divided by size of the array underlying type)
// std::size_t my_size = my_map.size();                                     //same as above, for STL compatibility
// bool is_empty = my_map.is_empty();                                       //returns true if map is ampty
// bool is_empty = my_map.empty();                                          //same as above, for STL compatibility
// unsigned char * my_data = my_map.get_data();                             //gets a raw pointer to file's data, whose underlying type is unsigned char (or the one designated at instantiation)
// unsigned char * my_data = my_map.data();                                 //same as above, for STL compatibility
// my_map.flush();                                                          //flushes data to file (unnecessary, since current process will be the only one accessing the file)
// 
// bool success = locker::is_locked("a.txt");                               //returns true if file is currently locked, false otherwise (throws if file does not exist)
// std::vector<std::string> my_locked = locker::get_locked();               //returns a vector with the canonical filenames of all currently locked files
// locker::clear();                                                         //unlocks all currently locked files (do not call this function if a lockfile is open)
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
		std::vector<std::string> filenames;
		
		public:
		
		lock_guard_t(lock_guard_t const &) = delete;
		lock_guard_t(lock_guard_t &&) = delete;
		auto & operator=(lock_guard_t) = delete;
		auto operator&() = delete;
		
		explicit lock_guard_t(std::vector<std::string> && fs) : filenames(std::forward<std::vector<std::string>>(fs))
		{
			lock(filenames);
		}
		
		explicit lock_guard_t(std::initializer_list<std::string> && fs) : filenames(std::forward<std::initializer_list<std::string>>(fs))
		{
			lock(filenames);
		}
		
		explicit lock_guard_t(std::string const & f) : filenames({f})
		{
			lock(filenames);
		}
		
		template <typename ... TS>
		explicit lock_guard_t(TS && ... f) : filenames({std::forward<TS>(f) ...})
		{
			lock(filenames);
		}
		
		~lock_guard_t()
		{
			unlock(filenames);
		}
	};
	
	template <typename data_t = unsigned char, std::enable_if_t<std::is_integral_v<data_t>> * dummy = nullptr>
	class [[nodiscard]] memory_map_t
	{
		std::string filename = "";
		int file_descriptor = -1;
		std::size_t data_size = 0;
		data_t * data_ptr = nullptr;
		
		public:
		
		memory_map_t(memory_map_t &) = delete;
		memory_map_t(memory_map_t &&) = delete;
		auto & operator=(memory_map_t) = delete;
		auto operator&() = delete;
		
		explicit memory_map_t(std::string const & f) : filename(f)
		{
			if(true)
			{
				if(filename.empty() or !std::filesystem::exists(filename) or !std::filesystem::is_regular_file(std::filesystem::status(filename)))
				{
					throw std::runtime_error("\"" + filename + "\" is not a regular file");
				}
				auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
				file_descriptor = unsafe_try_lock<true>(filename); //open(filename.c_str(), O_RDWR, 0666);
			}
			try
			{
				struct stat file_status;
				if(fstat(file_descriptor, &file_status) < 0)
				{
					throw std::runtime_error("could not get size of \"" + filename + "\"");
				}
				data_size = static_cast<std::size_t>(file_status.st_size / static_cast<off_t>(sizeof(data_t)));
				data_ptr = static_cast<data_t *>(mmap(nullptr, data_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, file_descriptor, 0));
				if(data_ptr == MAP_FAILED)
				{
					throw std::runtime_error("could not map file \"" + filename + "\" to memory");
				}
			}
			catch(...)
			{
				unlock(filename); //close(filename.c_str());
				data_ptr = nullptr;
				data_size = 0;
				file_descriptor = -1;
				filename = "";
				throw;
			}
		}
		
		~memory_map_t()
		{
			msync(data_ptr, data_size, MS_SYNC);
			munmap(data_ptr, data_size);
			unlock(filename);
			data_ptr = nullptr;
			data_size = 0;
			file_descriptor = -1;
			filename = "";
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
	
	std::mutex descriptors_mutex;
	std::map<std::string, std::pair<int, int>> descriptors;
	
	static auto & get_singleton()
	{
		static auto singleton = locker();
		return singleton;
	}
	
	template <bool should_block = false>
	static inline int unsafe_try_lock(std::string const & raw_filename)
	{
		mode_t mask = umask(0);
		int descriptor = open(raw_filename.c_str(), O_RDWR | O_CREAT, 0666);
		umask(mask);
		if(descriptor < 0)
		{
			throw std::runtime_error("could not open file \"" + raw_filename + "\"");
		}
		try
		{
			std::string const filename = std::filesystem::canonical(raw_filename);
			auto & descriptors = get_singleton().descriptors;
			if(descriptors.contains(filename))
			{
				close(descriptor);
				descriptors.at(filename).first += 1;
				return descriptors.at(filename).second;
			}
			if constexpr(should_block)
			{
				if(flock(descriptor, LOCK_EX) < 0)
				{
					throw std::runtime_error("could not lock file \"" + filename + "\"");
				}
			}
			else
			{
				if(flock(descriptor, LOCK_EX | LOCK_NB) < 0)
				{
					close(descriptor);
					return -1;
				}
			}
			descriptors.emplace(filename, std::make_pair(1, descriptor));
			return descriptor;
		}
		catch(...)
		{
			close(descriptor);
			throw;
		}
	}
	
	template <bool should_block = false>
	static inline bool unsafe_try_lock(std::vector<std::string> const & filenames)
	{
		for(std::size_t i = 0; i < filenames.size(); ++i)
		{
			try
			{
				if(unsafe_try_lock<should_block>(filenames[i]) < 0)
				{
					unsafe_unlock(std::vector<std::string>(filenames.begin(), std::next(filenames.begin(), static_cast<long>(i))));
					return false;
				}
			}
			catch(...)
			{
				unsafe_unlock(std::vector<std::string>(filenames.begin(), std::next(filenames.begin(), static_cast<long>(i))));
				throw;
			}
		}
		return true;
	}
	
	static inline void unsafe_unlock(std::string const & raw_filename)
	{
		if(!std::filesystem::exists(raw_filename))
		{
			throw std::runtime_error("could not find lockfile \"" + raw_filename + "\"");
		}
		std::string const filename = std::filesystem::canonical(raw_filename);
		auto & descriptors = get_singleton().descriptors;
		if(descriptors.contains(filename))
		{
			auto & descriptor = descriptors.at(filename);
			if((--descriptor.first <= 0) and ((fsync(descriptor.second) < 0) or (close(descriptor.second) < 0) or !descriptors.erase(filename)))
			{
				throw std::runtime_error("could not unlock file \"" + raw_filename + "\"");
			}
		}
	}
	
	static inline void unsafe_unlock(std::vector<std::string> const & filenames)
	{
		std::string missing = "";
		for(auto const & filename : filenames)
		{
			if(!std::filesystem::exists(filename))
			{
				missing += " \"" + filename + "\",";
			}
		}
		if(missing.size())
		{
			missing.pop_back();
			throw std::runtime_error("could not find lockfiles" + missing);
		}
		for(auto it = filenames.rbegin(); it != filenames.rend(); ++it)
		{
			std::string const filename = std::filesystem::canonical(*it);
			auto & descriptors = get_singleton().descriptors;
			if(descriptors.contains(filename))
			{
				auto & descriptor = descriptors.at(filename);
				if((--descriptor.first <= 0) and ((fsync(descriptor.second) < 0) or (close(descriptor.second) < 0) or !descriptors.erase(filename)))
				{
					throw std::runtime_error("could not unlock files \"" + filenames.front() + "\" to \"" + *it + "\"");
				}
			}
		}
	}
		
	locker() = default;
	
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
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		for(auto & descriptor : descriptors)
		{
			close(descriptor.second.second);
		}
		descriptors.clear();
	}
	
	static auto get_locked()
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		std::vector<std::string> filenames;
		for(auto && descriptor : get_singleton().descriptors)
		{
			filenames.emplace_back(descriptor.first);
		}
		return filenames;
	}
	
	static bool is_locked(std::string const & raw_filename)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		if(std::filesystem::exists(raw_filename))
		{
			std::string const filename = std::filesystem::canonical(raw_filename);
			return get_singleton().descriptors.contains(filename);
		}
		throw std::runtime_error("lockfile \"" + raw_filename + "\" does not exist");
	}
	
	static bool try_lock(std::string const & filename)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		return (unsafe_try_lock(filename) >= 0);
	}
	
	static bool try_lock(std::vector<std::string> const & filenames)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		return unsafe_try_lock(filenames);
	}
	
	static bool try_lock(std::initializer_list<std::string> && fs)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		return unsafe_try_lock(std::vector<std::string>(std::forward<std::initializer_list<std::string>>(fs)));
	}
	
	template <typename ... TS>
	static bool try_lock(std::string const & filename, TS && ... fs)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		return unsafe_try_lock(std::vector<std::string>({filename, std::forward<TS>(fs) ...}));
	}
	
	static void lock(std::string const & filename)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		unsafe_try_lock<true>(filename);
	}
	
	static void lock(std::vector<std::string> const & filenames)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		unsafe_try_lock<true>(filenames);
	}
	
	static void lock(std::initializer_list<std::string> fs)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		unsafe_try_lock<true>(std::vector<std::string>(std::forward<std::initializer_list<std::string>>(fs)));
	}
	
	template <typename ... TS>
	static void lock(std::string const & filename, TS && ... fs)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		unsafe_try_lock<true>(std::vector<std::string>({filename, std::forward<TS>(fs) ...}));
	}
	
	static void unlock(std::string const & filename)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		unsafe_unlock(filename);
	}
	
	static void unlock(std::vector<std::string> const & filenames)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		unsafe_unlock(filenames);
	}
	
	static void unlock(std::initializer_list<std::string> && fs)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		unsafe_unlock(std::vector<std::string>(std::forward<std::initializer_list<std::string>>(fs)));
	}
	
	template <typename ... TS>
	static void unlock(std::string const & filename, TS && ... fs)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		unsafe_unlock(std::vector<std::string>({filename, std::forward<TS>(fs) ...}));
	}
	
	template <typename ... TS>
	static auto lock_guard(TS && ... filenames)
	{
		return lock_guard_t(std::forward<TS>(filenames) ...);
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
		lock(filename);
		std::string data;
		try
		{
			auto input = std::fstream(filename, std::fstream::in | std::fstream::ate);
			if(!input.good())
			{
				throw std::runtime_error("could not open file \"" + filename + "\" for input");
			}
			data.resize(static_cast<std::size_t>(input.tellg()), '\n');
			input.seekg(0);
			input.read(data.data(), static_cast<long>(data.size()));
			while(data.size() and data.back() == '\n')
			{
				data.pop_back();
			}
		}
		catch(...)
		{
			unlock(filename);
			throw;
		}
		unlock(filename);
		return data;
	}

	template <typename T, std::enable_if_t<std::is_integral_v<T>> * dummy = nullptr>
	static auto xread(std::string const & filename)
	{
		if(!std::filesystem::exists(filename))
		{
			throw std::runtime_error("file \"" + filename + "\" does not exist");
		}
		lock(filename);
		std::vector<T> data;
		try
		{
			auto input = std::fstream(filename, std::fstream::in | std::fstream::ate | std::fstream::binary);
			if(!input.good())
			{
				throw std::runtime_error("could not open file \"" + filename + "\" for binary input");
			}
			data.resize(static_cast<std::size_t>(input.tellg()) / sizeof(T));
			input.seekg(0);
			input.read(reinterpret_cast<char *>(data.data()), static_cast<long>(data.size() * sizeof(T)));
		}
		catch(...)
		{
			unlock(filename);
			throw;
		}
		unlock(filename);
		return data;
	}

	template <bool should_append = false, typename ... TS>
	static auto xwrite(std::string const & filename, TS && ... data)
	{
		lock(filename);
		try
		{
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
			(output << ... << std::forward<TS>(data)) << std::flush;
		}	
		catch(...)
		{
			unlock(filename);
			throw;
		}
		unlock(filename);
	}

	template <bool should_append = false, typename T>
	static auto xflush(std::string const & filename, std::vector<T> const & data)
	{
		lock(filename);
		try
		{
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
		catch(...)
		{
			unlock(filename);
			throw;
		}
		unlock(filename);
	}

	template <bool should_append = false>
	static auto xflush(std::string const & filename, void * data, std::size_t const size)
	{
		lock(filename);
		try
		{
			auto flag = std::fstream::binary;
			if constexpr(should_append)
			{
				flag |= std::fstream::app;
			}
			auto output = std::ofstream(filename, flag);
			if(!output.good())
			{
				throw std::runtime_error("could not open file \"" + filename + "\" for binary output");
			}
			output.write(static_cast<char *>(data), static_cast<std::streamsize>(size));
			output.flush();
		}	
		catch(...)
		{
			unlock(filename);
			throw;
		}
		unlock(filename);
	}
};
