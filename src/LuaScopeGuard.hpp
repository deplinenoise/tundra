#ifndef TUNDRA_LUASCOPEGUARD_HPP
#define TUNDRA_LUASCOPEGUARD_HPP

struct lua_State;

namespace tundra
{

/*!
 * \brief Scope utility to verify that the Lua stack is balanced.
 *
 * Expands to nothing when NDEBUG is defined.
 */
#ifndef NDEBUG
class LuaScopeGuard
{
private:
	lua_State* const mState;
	int const mTop;
	const char * const mFile;
	int const mLine;

private:
	LuaScopeGuard(const LuaScopeGuard&);
	LuaScopeGuard& operator=(LuaScopeGuard&);

public:
	LuaScopeGuard(lua_State* state, const char* file, int line, int expectedFinish);

	~LuaScopeGuard();
};
#define TUNDRA_LUA_SCOPE_GUARD_SIMPLE(state_) \
	::tundra::LuaScopeGuard scope_(state_, __FILE__, __LINE__)
#define TUNDRA_LUA_SCOPE_GUARD_EXPECT(state_, expected_) \
	::tundra::LuaScopeGuard scope_(state_, __FILE__, __LINE__, expected_)
#else
#define TUNDRA_LUA_SCOPE_GUARD_SIMPLE(state_) do {} while(0)
#define TUNDRA_LUA_SCOPE_GUARD_EXPECT(state_, expected_) do {} while(0)
#endif

}

#endif
