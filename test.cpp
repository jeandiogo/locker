#include "locker.hpp"

#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <ranges>
#include <string>
#include <thread>

int main()
{
	auto const filename = "test.txt";
	std::ofstream(filename) << 0;
	
	int pid;
	for(auto i : std::views::iota(1, 11))
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
	std::cout << "\nPID " << getpid() << ":\ndata read: " << data << "\nincrementing...";
	std::ofstream(filename) << ++data;
	std::cout << "\ndata written: " << data << std::endl;
	
	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	
	return EXIT_SUCCESS;
}
