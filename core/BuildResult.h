#pragma 

#include "IStreamable.h"
#include <chrono>

namespace YAM
{
	class __declspec(dllexport) BuildResult : public IStreamable
	{
	public:
		BuildResult();
		BuildResult(IStreamer* reader) : IStreamable(reader) {}

		void succeeded(bool value);
		bool succeeded() const;

		// Return time of BuildResult construction
		std::chrono::system_clock::time_point startTime() const;
		// Return time of call to succeeded(value)
		std::chrono::system_clock::time_point endTime() const;
		// Return endTime() - startTime()
		std::chrono::system_clock::duration duration() const;

		static void setStreamableType(uint32_t type);
		// Inherited via IStreamable
		uint32_t typeId() const override;
		void stream(IStreamer* streamer) override;

	private:
		bool _succeeded;
		std::chrono::system_clock::time_point _startTime;
		std::chrono::system_clock::time_point _endTime;
	};
}


