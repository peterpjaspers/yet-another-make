#include "../NodeSet.h"
#include "NumberNode.h"

#include "gtest/gtest.h"
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <string>

namespace
{
	using namespace YAM;
	using namespace YAMTest;
	using namespace boost::process;

	void toLines(std::string const& str, std::vector<std::string>& outLines) {
		auto ss = std::stringstream(str);
		for (std::string line; std::getline(ss, line, '\n');) {
			if (!line.empty()) outLines.push_back(line);
		}
	}

	void toLines(std::vector<char> buf, std::vector<std::string>& outLines) {
		std::string str(buf.begin(), buf.end());
		toLines(str, outLines);
	}
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
	TEST(Process, asyncIO) {
		boost::asio::io_service ios;
		std::future<std::string> f_stdout;
		std::future<std::string> f_stderr;
		auto cmd = search_path("cmd");

		group g;
		child c(
			cmd.string() + " /c ping -n 3 localhost",
			g,
			std_out > f_stdout,
			std_err > f_stderr,
			ios);


		auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(15);
		ios.run_until(deadline);
		// n==3 => ping should take ~3 seconds. Use larger timeout.
		ASSERT_TRUE(g.wait_until(deadline));
		c.wait();
		ASSERT_EQ(0, c.exit_code());

		std::vector<std::string> stdoutLines;
		toLines(f_stdout.get(), stdoutLines);
		unsigned int nReplyLines = 0;
		for (auto const& line : stdoutLines) {
			if (line.starts_with("Reply from")) nReplyLines++;
		}
		EXPECT_EQ(3, nReplyLines);

		std::vector<std::string> stderrLines;
		toLines(f_stderr.get(), stderrLines);
		EXPECT_EQ(0, stderrLines.size());

	}

}