#pragma 

#include <chrono>

namespace YAM
{
	class __declspec(dllexport) BuildResult
	{
	public:
		BuildResult();

		void succeeded(bool value);
		bool succeeded() const;

		// Return time of BuildResult construction
		std::chrono::system_clock::time_point startTime() const;
		// Return time of call to succeeded(value)
		std::chrono::system_clock::time_point endTime() const;
		// Return endTime() - startTime()
		std::chrono::system_clock::duration duration() const;

	private:
		bool _succeeded;
		std::chrono::system_clock::time_point _startTime;
		std::chrono::system_clock::time_point _endTime;
	};
}


