#include "BasicOStreamLogBook.h"


namespace YAM
{
	BasicOStreamLogBook::BasicOStreamLogBook(std::basic_ostream<char, std::char_traits<char>>* ostream)
		: _ostream(ostream)
	{}

	void BasicOStreamLogBook::add(LogRecord const& record) {
		*_ostream
			<< record.time.wctime().date() << " " << record.time.wctime().time2() 
			<< " " << LogRecord::aspect2str(record.aspect)
			<< ": " << record.message << std::endl;
	} 
}