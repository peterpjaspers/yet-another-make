#include "..\core\ConsoleLogBook.h"

using namespace YAM;

int main(int argc, char** argv) {
	ConsoleLogBook book;
	LogRecord error(LogRecord::Error, "error test");
	LogRecord warning(LogRecord::Warning, "warning test");
	LogRecord progress(LogRecord::Progress, "progress test");
	LogRecord buildTime(LogRecord::BuildTimePrediction, "build time test");

	book.add(progress);
	book.add(warning);
	book.add(error);
	book.add(buildTime);

	return 0;
}