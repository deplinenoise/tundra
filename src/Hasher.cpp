#include "Hasher.hpp"
#include "Guid.hpp"
#include <cstring>

namespace tundra
{

Hasher::Hasher()
{
	MD5Init(&mContext);
}

void Hasher::AddString(const char* str)
{
	AddBytes(str, static_cast<int>(strlen(str)));
}

void Hasher::AddStrings(const char** str, int count)
{
	for (int i=0; i < count; ++i)
	{
		const char* s = str[i];
		AddBytes(s, static_cast<int>(strlen(s)));
	}
}

void Hasher::AddBytes(const void* data, int count)
{
	MD5Update(&mContext, (unsigned char*)data, static_cast<unsigned int>(count));
}

void Hasher::GetDigest(Guid* out_guid)
{
	MD5Final(out_guid->GetHash(), &mContext);
}

}