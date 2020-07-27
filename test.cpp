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

auto safe_open(std::string const & filename, std::ios_base::openmode const mode)
{
	auto file = std::fstream(filename, mode);
	if(!file.good())
	{
		throw std::runtime_error("could not open file \"" + filename + "\"");
	}
	return file;
}

int main()
{
	int data = 0;
	std::string const filename = "test.txt";
	safe_open(filename, std::fstream::out) << data << std::flush;
	std::cout << "Process " << getpid() << " initialized \"" << filename << "\" with 0 and expects " << NUM_FORKS << std::endl;
	
	for(std::size_t i = 0; i < NUM_FORKS; ++i)
	{
		auto pid = fork();
		if(pid < 0)
		{
			return EXIT_FAILURE;
		}
		else if(pid == 0)
		{
			break;
		}
		else if(i == NUM_FORKS - 1)
		{
			int status;
			while((pid = wait(&status)) > 0);
			auto const guard = locker::lock_guard(filename);
			safe_open(filename, std::fstream::in) >> data;
			std::cout << "The test was" << (data != NUM_FORKS ? " not" : "") << " successful!" << std::endl;
			return EXIT_SUCCESS;
		}
	}
	
	auto const guard = locker::lock_guard(filename);
	safe_open(filename, std::fstream::in) >> data;
	auto const inc_data = data + 1;
	safe_open(filename, std::fstream::out) << inc_data << std::flush;
	std::cout << "Child " << getpid() << " read " << data << " and wrote " << inc_data << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	return EXIT_SUCCESS;
}
