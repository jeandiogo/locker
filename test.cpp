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

#include "locker.hpp"

#define NUM_FORKS 50

int main()
{
	int data = 0;
	std::string const filename = "test.txt";
	std::ofstream(filename) << data;
	std::cout << "process " << getpid() << " initialized " << filename << " with " << data << "\nspawning " << NUM_FORKS << " incrementers:" << std::endl;
	
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
			std::ifstream(filename) >> data;
			auto const new_data = data + 1;
			std::ofstream(filename) << new_data;
			std::cout << "child " << getpid() << " read " << data << " and wrote " << new_data << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			return EXIT_SUCCESS;
		}
		else if(i == NUM_FORKS - 1)
		{
			int status = 0;
			while((pid = wait(&status)) > 0);
			auto const guard = locker::lock_guard(filename);
			std::ifstream(filename) >> data;
			std::cout << (data == NUM_FORKS ? "the test was successful!" : "the test has failed!") << std::endl;
			return EXIT_SUCCESS;
		}
	}
}
