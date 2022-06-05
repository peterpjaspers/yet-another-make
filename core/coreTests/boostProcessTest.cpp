#include "../NodeSet.h"
#include "NumberNode.h"

#include "gtest/gtest.h"
#include <boost/process.hpp>
#include <chrono>
#include <string>

namespace
{
	using namespace YAM;
	using namespace YAMTest;
	using namespace boost::process;

	TEST(Process, this_process) {
		auto env = boost::this_process::environment();
		auto path = env["Path"];
		EXPECT_NE("", path.to_string());
	}

	TEST(Process, ping) {
		ipstream stdoutOfPing;

		environment env;
		// empty environment not accepted by child (at least not on Windows).
		env["rubbish"] = "nonsense";

		auto ping = search_path("ping");
		// passing ping args in this way fails for some unknown reason...
		//child c(ping, "-n 3 127.0.0.1", g, std_out > stdoutOfPing);

		// ...while passing the args this way works.
		child c(ping.string() + " -n 3 127.0.0.1", env, std_out > stdoutOfPing);

		std::vector<std::string> data;
		std::string line;

		while (
			c.running() 
			&& std::getline(stdoutOfPing, line) 
			&& !line.empty()
		) {
			if (line.starts_with("Reply from")) data.push_back(line);
		}
		//ping should take ~n (==3) seconds. Use larger timeout.
		EXPECT_TRUE(c.wait_for(std::chrono::seconds(15)));
		EXPECT_EQ(3, data.size());
	}

	TEST(Process, pingInShell) {
		ipstream stdoutOfPing;

		auto cmd = search_path("cmd");
		auto ping = search_path("ping");
		group g;
		child c(cmd.string() + " /c ping -n 3 localhost", g, std_out > stdoutOfPing);

		std::vector<std::string> data;
		std::string line;

		while (
			c.running()
			&& std::getline(stdoutOfPing, line)
			&& !line.empty()
			) {
			if (line.starts_with("Reply from")) data.push_back(line);
		}
		// n==3 => ping should take ~3 seconds. Use larger timeout.
		EXPECT_TRUE(g.wait_for(std::chrono::seconds(15)));
		EXPECT_EQ(3, data.size());
	}

}