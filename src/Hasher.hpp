#ifndef TUNDRA_HASHER_HPP
#define TUNDRA_HASHER_HPP

#include "md5.h"

namespace tundra
{

class Guid;

class Hasher
{
private:
	MD5_CTX mContext;

public:
	Hasher();

public:
	void AddString(const char* str);
	void AddStrings(const char** str, int count);
	void AddBytes(const void* data, int count);

public:
	void GetDigest(Guid* out_guid);
};

}

#endif
