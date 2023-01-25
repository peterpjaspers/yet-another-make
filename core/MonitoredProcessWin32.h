#pragma once

#include "IMonitoredProcess.h"

#include <vector>
#include <future>
#include <boost/process.hpp>
#include <boost/asio.hpp>

namespace YAM
{

	class __declspec(dllexport) MonitoredProcessWin32 : public IMonitoredProcess
	{
	public:
		MonitoredProcessWin32(std::string const& program, std::map<std::string, std::string> env);

		MonitoredProcessResult const& wait() override;
		bool wait_for(unsigned int timoutInMilliSeconds) override;
		void terminate() override;

	private:
		boost::asio::io_service _ios;
		std::future<std::string> _stdout;
		std::future<std::string> _stderr;
		boost::process::group _group;
		boost::process::child _child;
		MonitoredProcessResult _result;
	};
}
