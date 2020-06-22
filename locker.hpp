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
// Locker is a header-only C++20 class with static member functions to lock files in Linux systems, so they can be accessed exclusively or used as inter-process mutexes. The locking policy works only among programs using this library, so locking a file does not prevent other processes from opening it, but it ensures that only one program will get the lock at a time.
// The locker does not provide thread-safety. Once a process has acquired the lock, neither its threads and future forks will be stopped by it, nor they will be able to mutually exclude each other by using the filelock. Therefore, avoid forking a program while it has some file locked, and use ordinary mutexes to synchronize its inner threads.
// If the file to be locked does not exist, it will be created. An exception will be throw if an empty filename is given, if a directory name is given, or if the program does not have permission to read and write the file and its directory. All locking and unlocking functions accept a single filename, a list of filenames, or a vector of filenames. If you have manually locked a file, do not forget to unlock it. Also, if for some reason you have locked a file twice, you have to unlock it twice too. Therefore, always prefer using the lock guard, which will automatically unlock the file before leaving its scope of declaration.
// Lock and unlock operations are independent from open and close operations, so if you want to open a lockfile, you need to use file handlers like "fstream" or "fopen", and close it before unlocking it. It is also your responsability to handle race conditions among threads that have opened a file locked by their parent. Therefore, instead of manually locking and opening a file, we suggest using the functions this library provides to perform exclusive read, write, append, and memory-map, which are all process-safe (although still not thread-safe) and will not interfere with your current locks.
// A process will loose the lock if the lockfile is deleted, so it may be a good practice to create separate (and hidden) lockfiles for each file you intend to use (e.g. to exclusively open "a.txt", lock the file ".lock.a.txt"). This will prevent you from losing the lock in case you need to erase and recreate the file without letting other processes get a lock to it. Do not forget to be consistent with the name of lockfiles throughout your programs.
// 
// (When compiling with g++ use the flag "-std=c++2a", available in GCC 7.0 or later.)
// 
// [Usage]
// 
// #include "locker.hpp"
// 
// bool success = locker::try_lock("a.lock");                               //tries to lock a file once, returns immediately
// bool success = locker::try_lock({"a.lock", "b.lock"});                   //tries to lock a list or a vector of files once, returns immediately
// 
// locker::lock("a.lock");                                                  //keeps trying to lock a file, only returns when file is locked
// locker::lock({"a.lock", "b.lock"});                                      //keeps trying to lock a list or a vector of files, only returns when all files are locked
// 
// locker::unlock("a.lock");                                                //unlocks a file if it is locked
// locker::unlock({"a.lock", "b.lock"});                                    //unlocks a list or a vector of files (in reverse order) if they are locked
// 
// locker::lock_guard_t my_lock = locker::lock_guard("a.lock");             //locks a file and automatically unlocks it before leaving current scope
// locker::lock_guard_t my_lock = locker::lock_guard({"a.lock", "b.lock"}); //locks a list or a vector of files and automatically unlocks them before leaving current scope
// 
// std::string my_data = locker::xread("a.txt");                            //exclusively-reads a file and returns its content as a string
// 
// locker::xwrite("a.txt", my_data);                                        //exclusively-writes formatted data to a file (data type must be insertable to std::fstream)
// locker::xwrite("a.txt", "value", ':', 42);                               //exclusively-writes multiple data to a file
// 
// locker::xappend("a.txt", my_data);                                       //exclusively-appends data to a file (data type must be insertable to std::fstream)
// locker::xappend("a.txt", "value", ':', 42);                              //exclusively-appends multiple data to a file
// 
// locker::memory_map_t my_map = locker::xmap("a.txt");                     //exclusively-maps a file to memory and returns a structure similar to an array of unsigned chars
// locker::memory_map_t my_map = locker::xmap<char>("a.txt");               //the type underlying the array can be chosen at instantiation via template argument
// unsigned char my_var = my_map.at(N);                                     //gets the N-th byte as an unsigned char, throws if file is smaller than or equal to N bytes
// unsigned char my_var = my_map[N];                                        //same, but does not check range
// my_map.at(N) = M;                                                        //assigns the value M to the N-th byte, throws if file is smaller than or equal to N bytes
// my_map[N] = M;                                                           //same, but does not check range
// std::size_t my_size = my_map.get_size();                                 //gets the size of the file
// std::size_t my_size = my_map.size();                                     //same as above, for STL compatibility
// unsigned char * my_data = my_map.get_data();                             //gets a raw pointer to file's data (whose type is designated at instantiation)
// unsigned char * my_data = my_map.data();                                 //same as above, for STL compatibility
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
#include <initializer_list>
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
		
		~lock_guard_t()
		{
			unlock(filenames);
		}
	};
	
	template <typename data_t>
	class [[nodiscard]] memory_map_t
	{
		std::string filename;
		int file_descriptor;
		std::size_t file_size;
		data_t * file_data;
		
		public:
		
		memory_map_t(memory_map_t &) = delete;
		memory_map_t(memory_map_t &&) = delete;
		auto & operator=(memory_map_t) = delete;
		auto operator&() = delete;
		
		explicit memory_map_t(std::string const & raw_filename) : filename(""), file_descriptor(-1), file_size(0), file_data(nullptr)
		{
			lock(raw_filename, filename, file_descriptor);
			try
			{
				struct stat file_status;
				if(fstat(file_descriptor, &file_status) < 0)
				{
					throw std::runtime_error("could not get size of \"" + filename + "\"");
				}
				file_size = static_cast<std::size_t>(file_status.st_size);
				file_data = static_cast<data_t *>(mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, file_descriptor, 0));
				if(!file_data)
				{
					throw std::runtime_error("could not map file \"" + filename + "\" to memory");
				}
			}
			catch(...)
			{
				unlock(filename);
				file_data = nullptr;
				file_size = 0;
				file_descriptor = -1;
				filename = "";
				throw;
			}
		}
		
		~memory_map_t()
		{
			msync(file_data, file_size, MS_SYNC);
			munmap(file_data, file_size);
			unlock(filename);
			file_data = nullptr;
			file_size = 0;
			file_descriptor = -1;
			filename = "";
		}
		
		auto & operator[](std::size_t index)
		{
			return file_data[index];
		}
		
		auto & at(std::size_t index)
		{
			if(index >= file_size)
			{
				throw std::runtime_error("index " + std::to_string(index) + " is out of the range [0, " + std::to_string(file_size) + "[ of \"" + filename + "\"");
			}
			return file_data[index];
		}
		
		auto get_data() const
		{
			return file_data;
		}
		
		auto data() const
		{
			return file_data;
		}
		
		auto get_size() const
		{
			return file_size;
		}
		
		auto size() const
		{
			return file_size;
		}
		
		auto flush()
		{
			if(msync(file_data, file_size, MS_SYNC) < 0)
			{
				return false;
			}
			return true;
		}
	};
	
	std::mutex descriptors_mutex;
	std::map<std::string, std::pair<int, int>> descriptors;
	
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
			throw std::runtime_error("lockfile name \"" + filename + "\" must not be empty");
		}
		if constexpr(should_not_create_path)
		{
			if(!std::filesystem::exists(filename))
			{
				throw std::runtime_error("lockfile \"" + filename + "\" does not exist");
			}
			return std::filesystem::canonical(filename);
		}
		else
		{
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
	}
	
	static bool try_lock(std::string const & raw_filename, std::string & filename, int & descriptor)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		filename = get_filename(raw_filename);
		if(descriptors.contains(filename))
		{
			descriptors.at(filename).first += 1;
			descriptor = descriptors.at(filename).second;
			return true;
		}
		mode_t mask = umask(0);
		descriptor = open(filename.c_str(), O_RDWR | O_CREAT, 0666);
		umask(mask);
		if(descriptor < 0)
		{
			descriptor = -1;
			filename = "";
			return false;
		}
		if(flock(descriptor, LOCK_EX | LOCK_NB) < 0)
		{
			close(descriptor);
			descriptor = -1;
			filename = "";
			return false;
		}
		try
		{
			descriptors.emplace(filename, std::make_pair(0, descriptor));
		}
		catch(...)
		{
			close(descriptor);
			descriptor = -1;
			filename = "";
			throw;
		}
		return true;
	}
	
	static void lock(std::string const & raw_filename, std::string & filename, int & descriptor)
	{
		while(!try_lock(raw_filename, filename, descriptor))
		{
		}
	}
	
	locker() = default;
	
	public:
	
	~locker()
	{
		for(auto & descriptor : descriptors)
		{
			close(descriptor.second.second);
		}
		descriptors.clear();
	}
	
	locker(locker const &) = delete;
	locker(locker &&) = delete;
	auto & operator=(locker) = delete;
	
	static bool try_lock(std::string const & raw_filename)
	{
		std::string filename = "";
		int descriptor = -1;
		return try_lock(raw_filename, filename, descriptor);
	}
	
	static bool try_lock(std::vector<std::string> const & filenames)
	{
		std::string filename = "";
		int descriptor = -1;
		for(std::size_t i = 0; i < filenames.size(); ++i)
		{
			if(!try_lock(filenames[i], filename, descriptor))
			{
				for(std::size_t j = i - 1; static_cast<long>(j) >= 0; --j)
				{
					unlock(filenames[j]);
				}
				descriptor = -1;
				filename = "";
				return false;
			}
		}
		descriptor = -1;
		filename = "";
		return true;
	}
	
	static void lock(std::string const & raw_filename)
	{
		std::string filename = "";
		int descriptor = -1;
		lock(raw_filename, filename, descriptor);
	}
	
	static void lock(std::vector<std::string> const & filenames)
	{
		std::string filename = "";
		int descriptor = -1;
		for(auto it = filenames.begin(); it != filenames.end(); ++it)
		{
			lock(*it, filename, descriptor);
		}
		descriptor = -1;
		filename = "";
	}
	
	static void lock(std::initializer_list<std::string> && filenames)
	{
		lock(std::vector<std::string>(std::forward<std::initializer_list<std::string>>(filenames)));
	}
	
	static void unlock(std::string const & raw_filename)
	{
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		auto const filename = get_filename<true>(raw_filename);
		if(descriptors.contains(filename))
		{
			auto & descriptor = descriptors.at(filename);
			if(descriptor.first > 0)
			{
				--descriptor.first;
			}
			else
			{
				if((fsync(descriptor.second) < 0) or (close(descriptor.second) < 0) or !descriptors.erase(filename))
				{
					throw std::runtime_error("could not unlock \"" + raw_filename + "\"");
				}
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
				auto & descriptor = descriptors.at(filename);
				if(descriptor.first > 0)
				{
					--descriptor.first;
				}
				else
				{
					if((fsync(descriptor.second) < 0) or (close(descriptor.second) < 0) or !descriptors.erase(filename))
					{
						throw std::runtime_error("could not unlock files \"" + filenames.front() + "\" to \"" + *it + "\"");
					}
				}
			}
		}
	}
	
	static void unlock(std::initializer_list<std::string> && filenames)
	{
		unlock(std::vector<std::string>(std::forward<std::initializer_list<std::string>>(filenames)));
	}
	
	static auto lock_guard(std::string const & filename)
	{
		return lock_guard_t(filename);
	}
	
	static auto lock_guard(std::vector<std::string> && filenames)
	{
		return lock_guard_t(std::forward<std::vector<std::string>>(filenames));
	}
	
	static auto lock_guard(std::initializer_list<std::string> && filenames)
	{
		return lock_guard_t(std::forward<std::initializer_list<std::string>>(filenames));
	}
	
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
			unlock(filename);
			return data;
		}
		catch(...)
		{
			unlock(filename);
			throw;
		}
	}
	
	template <typename ... TS>
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
			unlock(filename);
		}	
		catch(...)
		{
			unlock(filename);
			throw;
		}
	}
	
	template <typename ... TS>
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
			unlock(filename);
		}	
		catch(...)
		{
			unlock(filename);
			throw;
		}
	}
	
	template <typename data_t = unsigned char>
	static auto xmap(std::string const & filename)
	{
		return memory_map_t<data_t>(filename);
	}
};
