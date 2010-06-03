#include "StreamUtil.hpp"
#include <istream>
#include <ostream>

namespace tundra
{

std::ostream& operator<<(std::ostream& o, const EscapeString& str)
{
	const char* s = str.value;
	o << '"';
	for (;;)
	{
		if (const char ch = *s++)
		{
			if (ch == '\\' || ch == '"')
				o << '\\';
			o << ch;
		}
		else
			break;
	}
	o << '"';
	return o;
}

std::istream& operator>>(std::istream& i, const UnescapeString& str)
{
	char ch;
	i >> ch;
	if (!i || ch != '"')
	{
		i.setstate(std::ios_base::failbit);
		return i;
	}

	std::string& result = str.result;

	bool escape = false;
	while (i)
	{
		ch = (char) i.get();
		if (i.eof())
			break;
		if (!escape)
		{
			switch (ch)
			{
			case '"':
				return i;
				break;
			case '\\':
				escape = true;
				break;
			default:
				result.push_back(ch);
				break;
			}
		}
		else
		{
			result.push_back(ch);
		}
	}
	return i;
}

}