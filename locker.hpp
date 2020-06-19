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
// An exception will be throw if an empty filename is given, if a directory name is given, or if the program does not have permission to read from and write to the file and its directory. If the file to be locked does not exist, it will be created. All locking and unlocking functions are variadic, accepting a single filename, multiple filenames, a list of filenames, or a vector of filenames. If you have manually locked a file, do not forget to unlock it. Nevertheless, prefer using the lock guard, which will automatically unlock the file before leaving its scope of declaration.
// Be aware that lock and unlock operations are independent from open and close operations. If you want to open a lockfile, you need to use file handlers like "fstream" or "fopen", and close the file before unlocking it. It is also your responsability to handle race conditions among threads that have opened a file locked by their parent. Instead of manually locking and opening a file, we suggest using the functions this library provides to perform exclusive read, write, append, and memory-map, which are all process-safe (although still not thread-safe) and will not interfere with your current locks.
// Finally, a process will loose the lock if the lockfile is deleted. So it may be a good practice to create separate lockfiles for each file you intend to use (e.g. to exclusively open "a.txt", lock the file "a.txt.lock"). This will prevent you from losing the lock in case you need to erase and recreate the file without losing the lock to other processes. Do not forget to be consistent with the name of lockfiles throughout your programs.
// 
// (When compiling with g++ use the flag "-std=c++2a", available in GCC 7.0 or later.)
// 
// [Usage]
// 
// #include "locker.hpp"
// 
// bool success = locker::try_lock("a.lock");                               //tries to lock a file once, returns immediately
// bool success = locker::try_lock({"a.lock", "b.lock"});                   //tries to lock multiple files once, returns immediately
// 
// locker::lock("a.lock");                                                  //keeps trying to lock a file, only returns when file is locked
// locker::lock({"a.lock", "b.lock"});                                      //keeps trying to lock multiple files, only returns when files are locked
// 
// locker::lock(1, "a.lock");                                               //keeps trying to lock in intervals of approximately 1 millisecond
// locker::lock<std::chrono::nanoseconds>(1000, "a.lock");                  //use template argument to change the unit of measurement
// 
// locker::unlock("a.lock");                                                //unlocks a file if it is locked
// locker::unlock({"a.lock", "b.lock"});                                    //unlocks multiple files (in reverse order) if they are locked
// 
// locker::lock_guard_t my_lock = locker::lock_guard("a.lock");             //locks a file and automatically unlocks it before leaving current scope
// locker::lock_guard_t my_lock = locker::lock_guard({"a.lock", "b.lock"}); //locks multiple files and automatically unlocks them before leaving current scope
// 
// std::string my_data = locker::xread("a.txt");                            //exclusively reads a file and returns its content as a string
// std::string my_data = locker::xread<true>("a.txt");                      //same, but does not unlocks the file after the read (use this if the file was already lock before the call)
// 
// locker::xwrite("a.txt", my_data);                                        //exclusively writes data to a file (data type must be insertable to std::fstream)
// locker::xwrite<true>("a.txt", my_data);                                  //same, but does not unlocks the file after the write (use this if the file was already lock before the call)
// locker::xwrite("a.txt", "value", ':', 42);                               //exclusively writes multiple data to a file
// 
// locker::xappend("a.txt", my_data);                                       //exclusively appends data to a file (data type must be insertable to std::fstream)
// locker::xappend<true>("a.txt", my_data);                                 //same, but does not unlocks the file after the append (use this if the file was already lock before the call)
// locker::xappend("a.txt", "value", ':', 42);                              //exclusively appends multiple data to a file
// 
// locker::memory_map_t my_map = locker::xmap("a.txt");                     //exclusively maps a file to memory and returns a structure with a pointer to an array of unsigned chars
// locker::memory_map_t my_map = locker::xmap<true>("a.txt");               //same but does not unlock the file at destruction (use this if the file was already lock before the call)
// unsigned char my_var = my_map.at(N);                                     //gets the N-th byte as an unsigned char, throws file's content is smaller than N bytes
// unsigned char my_var = my_map[N];                                        //same, but does not check range
// my_map.at(N) = M;                                                        //assigns the value M to the N-th byte, throws if file's content is smaller than N bytes
// my_map[N] = M;                                                           //same, but does not check range
// std::size_t my_size = my_map.get_size();                                 //gets size of file's data
// unsigned char * my_array = my_map.get_data();                            //gets raw pointer to file's data, represented as an array of unsigned chars
// my_map.flush();                                                          //flushes data to file (unnecessary, since OS handles it automatically)
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <cerrno>
#include <cstdio>
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
		
		explicit lock_guard_t(std::vector<std::string> && fs) : filenames(fs)
		{
			lock(filenames);
		}
		
		template <typename ... TS>
		explicit lock_guard_t(TS && ... fs) : filenames({std::forward<TS>(fs) ...})
		{
			lock(filenames);
		}
		
		~lock_guard_t()
		{
			unlock(filenames);
		}
	};
	
	template <bool should_not_unlock = false>
	class [[nodiscard]] memory_map_t
	{
		std::string filename;
		int descriptor;
		std::size_t size;
		unsigned char * pointer;
		
		public:
		
		memory_map_t(memory_map_t &) = delete;
		memory_map_t(memory_map_t &&) = delete;
		auto & operator=(memory_map_t) = delete;
		auto operator&() = delete;
		
		explicit memory_map_t(std::string const & raw_filename) : filename(""), descriptor(-1), size(0), pointer(nullptr)
		{
			lock(raw_filename, filename, descriptor);
			try
			{
				struct stat file_status;
				if(fstat(descriptor, &file_status) < 0)
				{
					throw std::runtime_error("could not get size of \"" + filename + "\"");
				}
				size = static_cast<std::size_t>(file_status.st_size);
				pointer = static_cast<unsigned char *>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, descriptor, 0));
				if(!pointer)
				{
					throw std::runtime_error("could not map file \"" + filename + "\" to memory");
				}
			}
			catch(...)
			{
				if constexpr(!should_not_unlock)
				{
					unlock(filename);
				}
				filename = "";
				descriptor = -1;
				size = 0;
				pointer = nullptr;
				throw;
			}
		}
		
		~memory_map_t()
		{
			msync(pointer, size, MS_SYNC);
			munmap(pointer, size);
			if constexpr(!should_not_unlock)
			{
				unlock(filename);
			}
			filename = "";
			descriptor = -1;
			size = 0;
			pointer = nullptr;
		}
		
		auto & operator[](std::size_t index)
		{
			return pointer[index];
		}
		
		auto & at(std::size_t index)
		{
			if(index >= size)
			{
				throw std::runtime_error("index " + std::to_string(index) + " out of \"" + filename + "\" content range [0, " + std::to_string(size) + "[");
			}
			return pointer[index];
		}
		
		auto get_data() const
		{
			return pointer;
		}
		
		auto get_size() const
		{
			return size;
		}
		
		auto flush()
		{
			if(msync(pointer, size, MS_SYNC) < 0)
			{
				return false;
			}
			return true;
		}
	};
	
	std::mutex descriptors_mutex;
	std::map<std::string, int> descriptors;
	
	static auto & get_singleton()
	{
		static auto singleton = locker();
		return singleton;
	}
	
	static inline bool has_permissions(std::string const & filename)
	{
		struct stat file_info;
		if(stat(filename.c_str(), &file_info) < 0)
		{
			throw std::runtime_error("could not assert permissions of lockfile \"" + filename + "\": " + std::string(strerror(errno)));
		}
		auto permissions = std::filesystem::status(filename).permissions();
		auto const has_owner_permissions = file_info.st_uid == getuid() and (permissions & std::filesystem::perms::owner_read) != std::filesystem::perms::none and (permissions & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
		auto const has_group_permissions = file_info.st_gid == getgid() and (permissions & std::filesystem::perms::group_read) != std::filesystem::perms::none and (permissions & std::filesystem::perms::group_write) != std::filesystem::perms::none;
		auto const has_other_permissions = (permissions & std::filesystem::perms::others_read) != std::filesystem::perms::none and (permissions & std::filesystem::perms::others_write) != std::filesystem::perms::none;
		return has_owner_permissions or has_group_permissions or has_other_permissions;
	}
	
	template <bool should_not_create_path = false>
	static inline std::string get_filename(std::string const & filename)
	{
		if(filename.empty() or filename.back() == '/')
		{
			throw std::runtime_error("lockfile \"" + filename + "\" name must not be empty");
		}
		if(std::filesystem::exists(filename))
		{
			if(!std::filesystem::is_regular_file(std::filesystem::status(filename)))
			{	
				throw std::runtime_error("lockfile \"" + filename + "\" must be a regular file or non-existing");
			}
			if(!has_permissions(filename))
			{
				throw std::runtime_error("does not have permission to write in lockfile \"" + filename + "\"");
			}
			return std::filesystem::canonical(filename);
		}
		else
		{
			if constexpr(should_not_create_path)
			{
				throw std::runtime_error("lockfile \"" + filename + "\" does not exist");
			}
			else
			{
				std::string path = ".";
				std::string file = filename;
				for(std::size_t i = filename.size() - 1; static_cast<long>(i) >= 0; --i)
				{
					if(filename[i] == '/')
					{
						path = std::string(filename, 0, i);
						file = std::string(filename, i + 1, filename.size());
						break;
					}
				}
				if(!std::filesystem::exists(path) and !std::filesystem::create_directories(path))
				{
					throw std::runtime_error("could not create path to lockfile \"" + filename + "\"");
				}
				if(!has_permissions(path))
				{
					throw std::runtime_error("does not have permission to write in directory of lockfile \"" + filename + "\"");
				}
				return std::filesystem::canonical(path) / file;
			}
		}
	}
	
	locker() = default;
	
	public:
	
	~locker()
	{
		for(auto & descriptor : descriptors)
		{
			close(descriptor.second);
		}
		descriptors.clear();
	}
	
	locker(locker const &) = delete;
	locker(locker &&) = delete;
	auto & operator=(locker) = delete;
	
	static bool try_lock(std::string const & raw_filename, std::string & filename, int & descriptor)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		filename = get_filename(raw_filename);
		if(descriptors.contains(filename))
		{
			descriptor = descriptors.at(filename);
			return true;
		}
		mode_t mask = umask(0);
		descriptor = open(filename.c_str(), O_RDWR | O_CREAT, 0666);
		umask(mask);
		if(descriptor < 0)
		{
			filename = "";
			descriptor = -1;
			return false;
		}
		if(flock(descriptor, LOCK_EX | LOCK_NB) < 0)
		{
			close(descriptor);
			filename = "";
			descriptor = -1;
			return false;
		}
		try
		{
			descriptors.emplace(filename, descriptor);
		}
		catch(...)
		{
			close(descriptor);
			filename = "";
			descriptor = -1;
			throw;
		}
		return true;
	}
	
	static bool try_lock(std::string const & raw_filename)
	{
		std::string filename = "";
		int descriptor = -1;
		return try_lock(raw_filename, filename, descriptor);
	}
	
	static bool try_lock(std::vector<std::string> const & filenames)
	{
		for(std::size_t i = 0; i < filenames.size(); ++i)
		{
			if(!try_lock(filenames[i]))
			{
				for(std::size_t j = i - 1; static_cast<long>(j) >= 0; --j)
				{
					unlock(filenames[j]);
				}
				return false;
			}
		}
		return true;
	}
	
	static void lock(std::string const & raw_filename, std::string & filename, int & descriptor)
	{
		while(!try_lock(raw_filename, filename, descriptor))
		{
		}
	}
	
	static void lock(std::string const & raw_filename)
	{
		
		std::string filename = "";
		int descriptor = -1;
		lock(raw_filename, filename, descriptor);
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
	
	static void unlock(std::string const & raw_filename)
	{
		auto const filename = get_filename<true>(raw_filename);
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		if(descriptors.contains(filename))
		{
			auto const & descriptor = descriptors.at(filename);
			if((fsync(descriptor) < 0) or (close(descriptor) < 0) or !descriptors.erase(filename))
			{
				throw std::runtime_error("could not unlock \"" + raw_filename + "\"");
			}
		}
	}
	
	static void unlock(std::vector<std::string> const & filenames)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		for(auto it = filenames.rbegin(); it != filenames.rend(); ++it)
		{
			auto const filename = get_filename<true>(*it);
			if(descriptors.contains(filename))
			{
				auto const & descriptor = descriptors.at(filename);
				if((fsync(descriptor) < 0) or (close(descriptor) < 0) or !descriptors.erase(filename))
				{
					throw std::runtime_error("could not unlock \"" + *it + "\"");
				}
			}
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
	
	template <bool should_not_unlock = false>
	static auto xread(std::string const & filename)
	{
		lock(filename);
		try
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
			if constexpr(!should_not_unlock)
			{
				unlock(filename);
			}
			return data;
		}
		catch(...)
		{
			if constexpr(!should_not_unlock)
			{
				unlock(filename);
			}
			throw;
		}
	}
	
	template <bool should_not_unlock = false, typename ... TS>
	static auto xwrite(std::string const & filename, TS && ... data)
	{
		lock(filename);
		try
		{
			auto output = std::ofstream(filename);
			if(!output.good())
			{
				throw std::runtime_error("could not open file \"" + filename + "\" for output");
			}
			(output << ... << std::forward<TS>(data)) << std::flush;
			if constexpr(!should_not_unlock)
			{
				unlock(filename);
			}
		}	
		catch(...)
		{
			if constexpr(!should_not_unlock)
			{
				unlock(filename);
			}
			throw;
		}
	}
	
	template <bool should_not_unlock = false, typename ... TS>
	static auto xappend(std::string const & filename, TS && ... data)
	{
		lock(filename);
		try
		{
			auto output = std::fstream(filename, std::fstream::app);
			if(!output.good())
			{
				throw std::runtime_error("could not open file \"" + filename + "\" for append");
			}
			(output << ... << std::forward<TS>(data)) << std::flush;
			if constexpr(!should_not_unlock)
			{
				unlock(filename);
			}
		}	
		catch(...)
		{
			if constexpr(!should_not_unlock)
			{
				unlock(filename);
			}
			throw;
		}
	}
	
	template <bool should_not_unlock = false>
	static auto xmap(std::string const & filename)
	{
		return memory_map_t<should_not_unlock>(filename);
	}
};
