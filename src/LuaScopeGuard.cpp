#include "LuaScopeGuard.hpp"

#include <lua.h>
#include <cstdio>
#include <cstdlib>

namespace tundra
{

#ifndef NDEBUG
LuaScopeGuard::LuaScopeGuard(lua_State* state, const char* file, int line, int expectedFinish)
:	mState(state)
,	mTop(expectedFinish ? expectedFinish : lua_gettop(state))
,	mFile(file)
,	mLine(line)
{}

LuaScopeGuard::~LuaScopeGuard()
{
	if (lua_gettop(mState) != mTop)
	{
		fprintf(stderr, "%s:%d: expected stack top %d but found %d\n", mFile, mLine, mTop, lua_gettop(mState));
		abort();
	}
}
#endif

}

