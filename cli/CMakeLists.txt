# CMakeList.txt : CMake project for cli, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("cli")

# Add source to this project's executable.
add_executable (cli "cli.cpp" "cli.h" "OptionParser.h")

# TODO: Add tests and install targets if needed.
