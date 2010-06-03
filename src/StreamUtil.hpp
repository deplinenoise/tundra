#ifndef TUNDRA_STREAMUTIL_HPP
#define TUNDRA_STREAMUTIL_HPP

#include <iosfwd>
#include <string>

namespace tundra
{

struct EscapeString
{
	const char* const value;

	explicit EscapeString(const char* s) : value(s) {}
	explicit EscapeString(const std::string& s) : value(s.c_str()) {}

private:
	EscapeString& operator=(EscapeString&);
};

std::ostream& operator<<(std::ostream& o, const EscapeString& str);

struct UnescapeString
{
	mutable std::string& result;

	explicit UnescapeString(std::string& r) : result(r) {}

private:
	UnescapeString& operator=(UnescapeString&);
};

std::istream& operator>>(std::istream& i, const UnescapeString& str);

}
#endif
