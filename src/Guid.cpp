#include "Guid.hpp"
#include "md5.h"
#include <cstring>
#include <istream>
#include <ostream>
#include <stdexcept>

namespace tundra
{

Guid::Guid()
{
	memset(mData, 0, sizeof mData);
}

Guid::Guid(const void* data, int size)
{
	MD5_CTX ctx;
	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)(data), (unsigned int)size);
	MD5Final(&mData[0], &ctx);
}

Guid::Guid(MD5_CTX* context)
{
	MD5Final(&mData[0], context);
}

int Guid::CompareTo(const Guid& other) const
{
	return memcmp(mData, other.mData, sizeof mData);
}

bool operator==(const Guid& a, const Guid& b)
{
	return 0 == a.CompareTo(b);
}

bool operator!=(const Guid& a, const Guid& b)
{
	return 0 != a.CompareTo(b);
}

bool operator<(const Guid& a, const Guid& b)
{
	return a.CompareTo(b) < 0;
}

std::ostream& operator<<(std::ostream& o, const Guid& g)
{
	static char tohex[] = "0123456789abcdef";
	
	const u8* data = g.GetHash();

	for (int x=0; x<16; ++x)
	{
		const u8 byte = data[x];
		const u8 hi = (byte & 0xf0) >> 4;
		const u8 lo = (byte & 0xf);
		o << tohex[hi] << tohex[lo];
	}
	return o;
}

static inline u8 GetNybbleValue(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	else if (ch >= 'a' && ch <= 'f')
		return 0x0a + ch - 'a';
	else if (ch >= 'A' && ch <= 'F')
		return 0x0a + ch - 'A';
	else
		throw std::runtime_error("illegal nybble value");
}

std::istream& operator>>(std::istream& i, Guid& g)
{
	char textValue[33];
	i.width(sizeof textValue);
	i >> textValue;

	u8* const data = g.GetHash();

	for (int x=0; x<16; ++x)
	{
		u8 hi = GetNybbleValue(textValue[x*2]);
		u8 lo = GetNybbleValue(textValue[x*2 + 1]);
		data[x] = (hi << 4) | lo;
	}

	return i;
}

}
