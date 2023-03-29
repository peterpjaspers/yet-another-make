#include "gtest/gtest.h"
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <string>
#include <fstream>

namespace
{
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
        // child c(ping, "-n 1 127.0.0.1", g, std_out > stdoutOfPing);
        // child c(ping.string(), args({ " -n 3 127.0.0.1" }), env, std_out > stdoutOfPing);
        // Passing args as in previous lines fails because in these cases the args are
        // quoted ("") on the command line which is not accepted by ping.
        // Passing args, in what boost calls cmd style, as in next line works.
        child c(ping.string() + " -n 1 127.0.0.1", env, std_out > stdoutOfPing);

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
        c.wait();
        EXPECT_EQ(1, data.size());
    }

    TEST(Process, pingInShell) {
        ipstream stdoutOfPing;

        auto cmd = search_path("cmd");
        group g;
        child c(cmd.string() + " /c ping -n 1 127.0.0.1", g, std_out > stdoutOfPing);

        std::vector<std::string> data;
        std::string line;

        while (
            c.running()
            && std::getline(stdoutOfPing, line)
            && !line.empty()
            ) {
            if (line.starts_with("Reply from")) data.push_back(line);
        }
        // n==3 => ping should take ~1 seconds. Use larger timeout.
        EXPECT_TRUE(g.wait_for(std::chrono::seconds(10)));
        EXPECT_EQ(1, data.size());
    }
    TEST(Process, asyncIO) {
        boost::asio::io_service ios;
        std::future<std::string> f_stdout;
        std::future<std::string> f_stderr;
        auto cmd = search_path("cmd");

        group g;
        child c(
            cmd.string() + " /c ping -n 1 127.0.0.1",
            g,
            std_out > f_stdout,
            std_err > f_stderr,
            ios);


        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(10);
        ios.run_until(deadline);
        // n==1 => ping should take ~1 seconds. Use larger timeout.
        ASSERT_TRUE(g.wait_until(deadline));
        c.wait();
        ASSERT_EQ(0, c.exit_code());

        std::vector<std::string> stdoutLines;
        toLines(f_stdout.get(), stdoutLines);
        unsigned int nReplyLines = 0;
        for (auto const& line : stdoutLines) {
            if (line.starts_with("Reply from")) nReplyLines++;
        }
        EXPECT_EQ(1, nReplyLines);

        std::vector<std::string> stderrLines;
        toLines(f_stderr.get(), stderrLines);
        EXPECT_EQ(0, stderrLines.size());

    }

    TEST(Process, executeBatchFile) {
        boost::process::environment thisenv = boost::this_process::environment();
        boost::process::environment env;
        env["SystemRoot"] = thisenv.at("SystemRoot").to_string();
        //std::size_t nVars = thisenv.size();
        //std::size_t i = 0;
        //std::size_t start = (4*nVars / 8) + 11;
        //std::size_t end = (6*nVars / 8) - 1;

        //std::ofstream envStream("env.txt");
        //for (auto p : thisenv) {
        //    i++;
        //    if (i > start && i < end) {
        //        env.emplace(p.get_name(), p.to_string());
        //        envStream << p.get_name() << "=" << p.to_string() << std::endl;
        //    }
        //}
        //envStream.close();

        std::ofstream batchStream("batch.cmd");
        batchStream << "echo Hallo";
        batchStream.close();

        auto cmdExe = boost::process::search_path("cmd").string();
        std::string batch = cmdExe + " /c batch.cmd";
        std::string echo = cmdExe + " /c echo Hallo";

        child batch_no_env_ok(batch);
        batch_no_env_ok.wait();
        EXPECT_EQ(0, batch_no_env_ok.exit_code());

        child batch_env_fail(batch, env);
        batch_env_fail.wait();
        EXPECT_EQ(0, batch_env_fail.exit_code());

        child echo_no_env_ok(echo);
        echo_no_env_ok.wait();
        EXPECT_EQ(0, echo_no_env_ok.exit_code());

        child echo_env_ok(echo, env);
        echo_env_ok.wait();
        EXPECT_EQ(0, echo_env_ok.exit_code());
    }
}