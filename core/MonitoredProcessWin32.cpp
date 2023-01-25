#include "MonitoredProcessWin32.h"

#include <iostream>
#include <chrono>

namespace YAM
{
	MonitoredProcessWin32::MonitoredProcessWin32(std::string const& program, std::map<std::string, std::string> env)
		: IMonitoredProcess(program, env)
		, _child(
			program,
			_group,
			boost::process::std_out > _stdout, 
			boost::process::std_err > _stderr,
			_ios)
	{
	}

	MonitoredProcessResult const& MonitoredProcessWin32::wait() {
		_ios.run();
		_group.wait();
		_child.wait();
		_result.exitCode = _child.exit_code();
		_result.stdOut = _stdout.get();
		_result.stdErr = _stderr.get();
		return _result;
	}

	bool MonitoredProcessWin32::wait_for(unsigned int timoutInMilliSeconds) {
		auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(timoutInMilliSeconds);
		_ios.run_until(deadline);
		return _group.wait_until(deadline);
	}

	void MonitoredProcessWin32::terminate() {
	    _group.terminate();
	}
}