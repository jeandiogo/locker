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
// test.cpp
// 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#define runtime_error(x) runtime_error("[" + std::string(__FILE__) + ":" + std::string(__func__) + ":" + std::to_string(__LINE__) + "] " + (x))

#include "locker.hpp"

#define NUM_FORKS 50

int main()
{
	auto const filename = "test.txt";
	std::ofstream(filename) << 0;
	std::cout << "\"" << filename << "\" was initialized with 0 and should be incremented up until " << NUM_FORKS << std::endl;
	
	int pid;
	for(std::size_t i = 0; i < NUM_FORKS; ++i)
	{
		pid = fork();
		if(pid < 0)
		{
			return EXIT_FAILURE;
		}
		else if(pid == 0)
		{
			break;
		}
	}
	if(pid > 0)
	{
		int status;
		while((pid = wait(&status)) > 0)
		{
		}
		return EXIT_SUCCESS;
	}
	
	auto guard = locker::lock_guard(filename);
	
	int data;
	std::ifstream(filename) >> data;
	std::cout << "PID " << getpid() << " read " << data << ", wrote ";
	++data;
	std::ofstream(filename) << data << std::flush;
	std::cout << data << std::endl;
	
	if(data == NUM_FORKS)
	{
		std::cout << "The test was successful!" << std::endl;
	}
	
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	
	return EXIT_SUCCESS;
}
