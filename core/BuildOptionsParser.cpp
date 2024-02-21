#include "BuildOptionsParser.h"
#include "../cli/OptionParser.h"

#include <iostream>
#include <string>
#include <vector>

namespace YAM
{
    enum  optionIndex { UNKNOWN, HELP, CLEAN };
    const option::Descriptor usage[] =
    {
     {UNKNOWN, 0, "", "",option::Arg::None, "USAGE: yam [options] [ -- files ] \n\n"
                                            "Options:" },
     {HELP, 0, "", "help",option::Arg::None, "  --help  \tPrint usage and exit." },
     {CLEAN, 0, "", "clean",option::Arg::None, "  --clean  \tDelete specified output files" },
     {UNKNOWN, 0, "", "",option::Arg::None, "\nExamples:\n"
                                   "  yam --clean\n"
                                   "  yam -- bin/main.obj bin/lib.obj\n" },
     {0,0,0,0,0,0}
    };
}

namespace YAM
{
    BuildOptionsParser::BuildOptionsParser(
        int argc, char* argv[],
        BuildOptions& buildOptions
    ) {
        argc -= (argc > 0); argv += (argc > 0); // skip program name argv[0] if present
        option::Stats  stats(usage, argc, argv);
        std::vector<option::Option> options(stats.options_max);
        std::vector<option::Option> buffer(stats.buffer_max);
        option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);

        _parseError = parse.error();
        if (!_parseError) {
            int nUnknowns = 0;
            for (option::Option* opt = options[UNKNOWN]; opt; opt = opt->next(), nUnknowns++) {
                std::cout << "Unknown option: " << std::string(opt->name, opt->namelen) << "\n";
            }
            _parseError = 0 < nUnknowns;
            if (_parseError) option::printUsage(std::cout, usage);
        }
        if (!_parseError) {
            if (options[HELP]) option::printUsage(std::cout, usage);
            if (options[CLEAN]) buildOptions._clean = true;
            for (int i = 0; i < parse.nonOptionsCount(); ++i) {
                buildOptions._scope.push_back(parse.nonOption(i));
            }
        }
    }
}
