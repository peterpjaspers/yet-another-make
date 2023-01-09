#include "MSBuildTrackerOutputReader.h"

#include <regex>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>

namespace
{
	std::regex readPattern(".*\\.read\\.\\d+\\.tlog$");
	std::regex writePattern(".*\\.write\\.\\d\\.tlog$");

	char bom_utf8[3]   { (char)(0xEF), (char)(0xBB), (char)(0xBF)             };
	char bom_utf16be[2]{ (char)(0xFE), (char)(0xFF)                           };
	char bom_utf16le[2]{ (char)(0xFF), (char)(0xFE)                           };
	char bom_utf32be[4]{ (char)(0   ), (char)(0   ), (char)(0xFF), char(0xFE) };
	char bom_uff32le[4]{ (char)(0xFF), (char)(0xFE), (char)(0   ), char(0   ) };

	// Read entire content of given file in memory buffer.
	// Post: length contains number of bytes in buffer.
	// Returned buffer must be freed (using delete[]) by caller.
	// Return nullptr on error.
	char* readFile(std::filesystem::path const& path, std::streampos& length) {
		char* buffer = nullptr;
		length = 0;
		std::ifstream file(path, std::ios::binary);
		if (file.is_open()) {
			file.seekg(0, std::ios::end);
			length = file.tellg();
			file.seekg(0, std::ios::beg);
			buffer = new char[length];
			file.read(buffer, length);
			file.close();
		}
		return buffer;
	}

	// Because all std::codecvt_* variants are deprecated since C++17 I had
	// to revert to this naive implementation of reading a utf16-le file into
	// a std::wstring. This implementation only works for platforms that have 
	// 16-bit chars in std::wstring in utf16 encoding. Which happens to be the
	// case for Windows when using the Microsoft C++ compiler.
	std::wstring readEntireUTF16LEFile(std::filesystem::path const& path) {
		std::streampos length;
		char* buffer = readFile(path, length);
		if (buffer != nullptr) {
			// verify ByteOrderMark (BOM)
			if (
				length % 2 != 0
				|| length < 2 
				|| buffer[0] != bom_utf16le[0] 
				|| buffer[1] != bom_utf16le[1]
			) {
				throw std::exception("Unsupported BOM in tracker logfile");
			}
			// skip BOM
			std::u16string u16str((char16_t*)(buffer + 2), (length / 2) - 1);
			std::wstring wstr(u16str.begin(), u16str.end());
			delete[] buffer;
			return wstr;
		}
		return std::wstring();
	}

	std::wstring u16_to_wstring(const std::u16string& u16str)
	{
	    std::wstring wstr( u16str.begin(), u16str.end() );
		return(wstr);
	}
}

namespace YAM
{
	// Read the Tracker.exe logfiles from the given directory.
	MSBuildTrackerOutputReader::MSBuildTrackerOutputReader(std::filesystem::path const& logDir) {
		auto di = std::filesystem::directory_iterator(logDir);
		for (auto const& dir_entry : di)
		{
			std::string p = dir_entry.path().string();
			if (std::regex_search(p, readPattern)) {
				parseDependencies(p, _readFiles);
			} else if (std::regex_search(p, writePattern)) {
				parseDependencies(p, _writtenFiles);
			}
		}
		std::set_difference(
			_readFiles.begin(), _readFiles.end(),
			_writtenFiles.begin(), _writtenFiles.end(),
			std::inserter(_readOnlyFiles, _readOnlyFiles.begin()));
	}

	void MSBuildTrackerOutputReader::parseDependencies(
		std::filesystem::path const& logFile, 
		std::set<std::filesystem::path>& dependencies) {

		// This code assumes that tracker log files are written with
		// the default Windows 10 UTF16-LE encoding.
		std::wstring wstr = readEntireUTF16LEFile(logFile);
		std::wstringstream ws(wstr);
		std::wstring line;
		while (std::getline(ws, line)) {
			line.erase(line.find_last_not_of(L" \t\r") + 1);
			if (line[0] != L'#' && line[0] != L'^') {
				dependencies.emplace(std::filesystem::path(line));
			}
		}	
	}

	std::set<std::filesystem::path> const& MSBuildTrackerOutputReader::readFiles() const {
		return _readFiles;
	}
	std::set<std::filesystem::path> const& MSBuildTrackerOutputReader::writtenFiles() const {
		return _writtenFiles;
	}
	std::set<std::filesystem::path> const& MSBuildTrackerOutputReader::readOnlyFiles() const {
		return _readOnlyFiles;
	}
}
;