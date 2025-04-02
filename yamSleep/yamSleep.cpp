// yamSleep.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
#include <string>

int main(int argc, char** argv)
{
	int count = 1000;
	if (argc > 1) {
		count = std::stoi(argv[1]);
	}
	bool report = (argc > 2);
	if (count > 0) {
		std::chrono::milliseconds interval(count);
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	}
	if (report) std::cout << "Slept for " << count << " milliseconds" << std::endl;
}
	