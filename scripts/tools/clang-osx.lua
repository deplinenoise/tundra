
local env = ...

load_toolset("gcc-osx", env)

env:set_many {
	["CC"] = "clang",
	["C++"] = "clang",
	["LD"] = "clang",
}
