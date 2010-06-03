#ifndef TUNDRA_GUID_HPP
#define TUNDRA_GUID_HPP

#include "Portable.hpp"
#include "md5.h"
#include <iosfwd>

namespace tundra
{

class Guid
{
private:
	u8 mData[16];

public:
	/// Return the hash digest this guid represents.
	const u8* GetHash() const { return mData; }

	/// Return the hash digest buffer.
	u8* GetHash() { return mData; }

public:
	/// Default constructor.
	Guid();

	/// Construct from an MD5 context
	explicit Guid(MD5_CTX* context);

	/// Construct from data to be hashed.
	Guid(const void* data, int size);

public:
	/// Memcmp-like comparison.
	int CompareTo(const Guid& other) const;
};

bool operator==(const Guid& a, const Guid& b);
bool operator!=(const Guid& a, const Guid& b);
bool operator<(const Guid& a, const Guid& b);

std::ostream& operator<<(std::ostream&, const Guid&);
std::istream& operator>>(std::istream&, Guid&);

}

#endif
