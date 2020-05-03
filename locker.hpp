////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Locker (C++20 library)
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
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
// Locker is a header-only C++20 class with static member functions to lock files in Linux systems, so they can be used as inter-process mutexes.
// Be aware that locking a file does not prevent other programs from reading or writing to it. The locking policy works only among programs using this library.
// Be also aware that the locking does not prevent data races among threads of the current process. Use mutexes to perform exclusive reading/writing of files inside your program.
// All methods will throw an exception if an empty filename is given or if the program does not have permission to write to the file or to the directory the file is stored.
// If the file to be locked does not exist, it will be created.
// After locking a file, you still need to open it using the input/output method you preffer. Do not forget to close the file before unlocking it.
// Actually, there are helper functions to lock-open, to unlock-close, to lock-open-read-close-unlock, and to lock-open-write-close-unlock. And they are all thread-safe.
// There are also guards for the locks and for the opening of files. Prefer them over manual locking/unlocking and opening/closing.
// Nevertheless, it may be a good practice to create a separate lockfile for each file you intend to use (e.g. to open "a.txt" exclusively, lock-guard the file "a.txt.lock").
// 
// (To compile with GCC, use the flag -std=c++2a.)
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Usage:
// 
// #include "locker.hpp"
// 
// bool success = locker::try_lock("a.lock");                           //tries to lock a file, returns immediately
// bool success = locker::try_lock("a.lock", "b.lock");                 //tries to lock multiple files, returns immediately
// bool success = locker::try_lock({"a.lock", "b.lock"});               //arguments can also be sent as a list or a vector of filenames
// 
// locker::lock("a.lock");                                              //only returns when the file is locked
// locker::lock("a.lock", "b.lock");                                    //only returns when all files are locked
// locker::lock({"a.lock", "b.lock", "c.lock"});                        //arguments can also be sent as a list or a vector of filenames
// 
// locker::unlock("a.lock");                                            //unlocks a file (if locked)
// locker::unlock("a.block", "b.lock");                                 //unlocks files in reverse order of function arguments (same as unlock<false>)
// locker::unlock<true>("a.block", "b.lock", "c.lock");                 //set template argument to unlock in strict order of function arguments
// locker::unlock({"a.block", "b.lock", "c.lock"});                     //arguments can also be sent as a list or a vector of filenames
// 
// auto my_lock = locker::lock_guard("a.lock");                         //locks a file and automatically unlocks it when leaving current scope
// auto my_lock = locker::lock_guard("a.lock", "b.lock");               //locks multiple files and automatically unlocks them when leaving current scope
// auto my_lock = locker::lock_guard({"a.lock", "b.lock"});             //arguments can also be sent as a list or a vector of filenames
// 
// std::fstream & my_stream = locker::xopen("a.txt");                   //locks a file, opens it, and returns a reference to its stream
// std::fstream & my_stream = locker::xopen("a.txt", std::fstream::in); //the opening mode can be specified as a function argument
// 
// locker::xclose("a.txt");                                             //closes and unlocks a file
// locker::xclose("a.txt", "b.txt");                                    //closes and unlocks multiple files
// 
// auto my_guard = locker::open_guard("a.txt", std::fstream::out);      //this will xopen a file and automatically xclose it when leaving current scope
// std::fstream & my_stream = my_guard.stream;                          //the stream is available as a public member
// 
// std::string my_data = locker::xread("a.txt");                        //this will exclusively read a file and return its content as a string
// locker::xwrite("a.txt", my_data);                                    //this will exclusively write a string (or any insertable type) to a file
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
	std::map<std::string, int> descriptors;
	std::map<std::string, std::fstream> streams;
	std::mutex descriptors_mutex;
	std::mutex streams_mutex;
	
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
		for(auto const & descriptor : descriptors)
		{
			close(descriptor.second);
		}
	}
	
	locker(locker const &) = delete;
	locker(locker &&) = delete;
	auto & operator=(locker) = delete;
	
	template <bool dummy = false>
	static auto unlock(std::string const & filename)
	{
		if(filename.empty())
		{
			return;
		}
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & fds = get_singleton().descriptors;
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
			unlock<should_not_reverse>(filename);
			unlock<should_not_reverse>(std::forward<TS>(filenames) ...);
		}
		else
		{
			unlock<should_not_reverse>(std::forward<TS>(filenames) ...);
			unlock<should_not_reverse>(filename);
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
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().descriptors_mutex);
		auto & descriptors = get_singleton().descriptors;
		if(descriptors.contains(filename))
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
	
	static auto & xopen(std::string const & filename, std::ios_base::openmode mode = std::fstream::app)
	{
		lock(filename);
		auto const guard = std::scoped_lock<std::mutex>(get_singleton().streams_mutex);
		auto & streams = get_singleton().streams;
		if(streams.contains(filename))
		{
			if(streams.at(filename).is_open())
			{
				streams.at(filename).flush();
				streams.at(filename).close();
			}
			streams.erase(filename);
		}
		streams[filename] = std::fstream(filename, mode);
		if(!streams.at(filename).good())
		{
			streams.erase(filename);
			unlock(filename);
			throw std::runtime_error("could not xopen file \"" + filename + "\"");
		}
		return streams.at(filename);
	}
	
	template <bool dummy = false>
	static auto xclose(std::string const & filename)
	{
		if(filename.size())
		{
			auto const guard = std::scoped_lock<std::mutex>(get_singleton().streams_mutex);
			auto & streams = get_singleton().streams;
			if(streams.contains(filename))
			{
				if(streams.at(filename).is_open())
				{
					streams.at(filename).flush();
					streams.at(filename).close();
				}
				streams.erase(filename);
				unlock(filename);
			}
		}
	}
	
	template <bool should_not_reverse = false, typename ... TS>
	static auto xclose(std::string const & filename, TS && ... filenames)
	{
		if constexpr(should_not_reverse)
		{
			xclose<should_not_reverse>(filename);
			xclose<should_not_reverse>(std::forward<TS>(filenames) ...);
		}
		else
		{
			xclose<should_not_reverse>(std::forward<TS>(filenames) ...);
			xclose<should_not_reverse>(filename);
		}
	}
	
	class open_guard
	{
		std::string filename;
		
		public:
		
		std::fstream & stream;
		
		open_guard(open_guard const &) = delete;
		open_guard(open_guard &&) = delete;
		auto & operator=(open_guard) = delete;
		auto operator&() = delete;
		
		open_guard(std::string const & f, std::ios_base::openmode mode = std::fstream::app) : filename(f), stream(xopen(filename, mode))
		{
		}
		
		~open_guard()
		{
			xclose(filename);
		}
	};
	
	static auto xread(std::string const & filename)
	{
		auto guard = open_guard(filename, std::fstream::in);
		auto & input = guard.stream;
		std::string data;
		input.seekg(0, std::ios::end);
		data.reserve(static_cast<std::size_t>(input.tellg()));
		input.seekg(0, std::ios::beg);
		data.assign((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
		if(data.size() and data.back() == '\n')
		{
			data.pop_back();
		}
		return data;
	}
	
	template <typename T>
	static auto xwrite(std::string const & filename, T const & data)
	{
		auto output = open_guard(filename, std::fstream::out);
		output.stream << data << std::flush;
	}
};
