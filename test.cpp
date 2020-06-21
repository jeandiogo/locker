#include "locker.hpp"

#include <chrono>
#include <string>
#include <thread>

int main()
{
	auto const filename = "test.txt";
	
	auto const guard = locker::lock_guard(filename);
	
	auto data = std::stoi(locker::xread<true>(filename));
	
	locker::xwrite<true>(filename, ++data);
	
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
