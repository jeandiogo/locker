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
// test.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#define runtime_error(x) runtime_error("[" + std::string(__FILE__) + ":" + std::string(__func__) + ":" + std::to_string(__LINE__) + "] " + (x))

#include "locker.hpp"

#define NUM_FORKS 50

inline auto safe_open(std::string const & filename, std::ios_base::openmode const mode)
{
	auto file = std::fstream(filename, mode);
	if(!file.good())
	{
		throw std::runtime_error("could not open file '" + filename + "'");
	}
	return file;
}

int main()
{
	int data = 0;
	std::string const filename = "test.txt";
	safe_open(filename, std::fstream::out) << data << std::flush;
	std::cout << "Process " << getpid() << " initialized file '" << filename << "' with value '" << data << "'\n";
	std::cout << "Spawning " << NUM_FORKS << " children to increment the value\n";
	std::cout << std::flush;
	
	for(std::size_t i = 0; i < NUM_FORKS; ++i)
	{
		auto pid = ::fork();
		if(pid < 0)
		{
			throw std::runtime_error("fork did not work");
		}
		else if(pid == 0)
		{
			auto const guard = locker::lock_guard(filename);
			safe_open(filename, std::fstream::in) >> data;
			auto const new_data = data + 1;
			safe_open(filename, std::fstream::out) << new_data << std::flush;
			std::cout << "Child " << getpid() << " read '" << data << "' and wrote '" << new_data << "'\n" << std::flush;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			return EXIT_SUCCESS;
		}
		else if(i == NUM_FORKS - 1)
		{
			int status = 0;
			while((pid = wait(&status)) > 0);
			auto const guard = locker::lock_guard(filename);
			safe_open(filename, std::fstream::in) >> data;
			std::cout << (data == NUM_FORKS ? "The test was successful!\n" : "The test has failed!\n") << std::flush;
			return EXIT_SUCCESS;
		}
	}
}
