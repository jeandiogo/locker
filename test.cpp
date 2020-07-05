// Locker (C++ Library)
// Copyright (C) 2020 Jean Diogo (Jango) <jeandiogo@gmail.com>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <ranges>
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
	for(auto i : std::views::iota(0, NUM_FORKS))
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
