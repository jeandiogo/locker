#include "locker.hpp"

#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <ranges>
#include <string>
#include <thread>

#define NUM_FORKS 10

int main()
{
	auto const filename = "test.txt";
	std::ofstream(filename) << 0;
	std::cout << "\"" << filename << "\" initialized with 0 and should be incremented by " << NUM_FORKS << std::endl;
	
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
		return EXIT_SUCCESS;
	}
	
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	
	auto guard = locker::lock_guard(filename);
	
	int data;
	std::ifstream(filename) >> data;
	std::cout << "\nPID " << getpid() << " read " << data << ", wrote ";
	++data;
	std::ofstream(filename) << data << std::flush;
	std::cout << data << std::flush;
	
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	
	if(data == NUM_FORKS)
	{
		std::cout << "\nThe test was successful!" << std::endl;
	}
	
	return EXIT_SUCCESS;
}
