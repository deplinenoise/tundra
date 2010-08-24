-- Copyright 2010 Andreas Fredriksson
--
-- This file is part of Tundra.
--
-- Tundra is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- Tundra is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with Tundra.  If not, see <http://www.gnu.org/licenses/>.

-- tundra.lua - Self-hosting build file for Tundra itself

local common = {
	Env = {
		CPPPATH = { "src", "lua/src" },
		LUAC = "$(OBJECTDIR)/tundra_luac$(HOSTPROGSUFFIX)",
		GEN_LUA_DATA = "$(OBJECTDIR)/gen_lua_data$(HOSTPROGSUFFIX)",
	},
}

local common_win32 = {
	Inherit = common,
	Env = {
		CCOPTS = { "/WX", "/wd4127", "/wd4100", "/wd4324" },
		CPPDEFS = { "_CRT_SECURE_NO_WARNINGS" },
		LIBS = { "kernel32.lib", "advapi32.lib" },
	},
}

Build {
	Units = "units.lua",
	Passes= {
		CodeGen = { Name = "Code generation", BuildOrder = 1 },
		LuaCompile = { Name = "Compile Lua sources for embedding", BuildOrder = 2 },
		Tundra = { Name = "Main compile pass", BuildOrder = 3 },
	},
	SyntaxExtensions = { "glob", "embed_lua" },
	Configs = {
		Config { Name = "macosx-clang", Inherit = common, Tools = { "clang-osx" }, DefaultOnHost = "macosx" },
		Config { Name = "macosx-gcc", Inherit = common, Tools = { "gcc-osx" } },
		Config { Name = "win32-msvc", Inherit = common_win32, Tools = { { "msvc-vs2008"; TargetArch = "x86"} } },
		Config { Name = "win64-msvc", Inherit = common_win32, Tools = { { "msvc-vs2008"; TargetArch = "x64"} } },
		Config { Name = "linux-gcc", Inherit = common, Tools = { "gcc" } },

		-- MingW32 cross compilation under OS X
		Config {
			Name = "macosx-mingw32",
			Inherit = common,
			Tools = { "gcc" },
			Env = {
				_GCC_BINPREFIX="/usr/local/i386-mingw32-4.3.0/bin/i386-mingw32-",
				CCOPTS = "-Werror",
			},
			ReplaceEnv = {
				PROGSUFFIX=".exe",
				SHLIBSUFFIX=".dll",
			},
		},
	},
	SubVariants = { "dev", "standalone" },
	DefaultSubVariant = "dev",
}

