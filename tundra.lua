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
		LUAC = "$(OBJECTDIR)$(SEP)tundra_luac$(HOSTPROGSUFFIX)",
		GEN_LUA_DATA = "$(OBJECTDIR)$(SEP)gen_lua_data$(HOSTPROGSUFFIX)",
		CPPPATH = { "src", "lua/src" },
		CCOPTS = {
			{ "/W4", "/WX", "/wd4127", "/wd4100", "/wd4324"; Config = "*-msvc-*" },
			{ "-Wall", "-Werror"; Config = { "*-gcc-*", "*-clang-*", "*-crosswin32" } },
			{ "-g"; Config = { "*-gcc-debug", "*-clang-debug", "*-gcc-production", "*-clang-production" } },
			{ "-O2"; Config = { "*-gcc-production", "*-clang-production", "*-crosswin32-production" } },
			{ "-O3"; Config = { "*-gcc-release", "*-clang-release", "*-crosswin32-release" } },
			{ "/O2"; Config = "*-msvc-production" },
			{ "/Ox"; Config = "*-msvc-release" },
		},
		CPPDEFS = {
			{ "_CRT_SECURE_NO_WARNINGS"; Config = "*-msvc-*" },
			{ "_GNU_SOURCE"; Config = "linux-*" },
			{ "NDEBUG"; Config = "*-*-release" },
			{ "TD_STANDALONE"; Config = "*-*-*-standalone" },
		},
	}
}

local function genwincfg(name, arch, vcversion)
	return Config {
		Name = name,
		Inherit = common,
		Tools = {
			{ "msvc-winsdk"; TargetArch = arch, VcVersion = vcversion }
		}
	}
end

Build {
	Units = "units.lua",
	Passes= {
		CodeGen = { Name = "Code generation", BuildOrder = 1 },
		LuaCompile = { Name = "Compile Lua sources for embedding", BuildOrder = 2 },
		Tundra = { Name = "Main compile pass", BuildOrder = 3 },
	},
	SyntaxExtensions = { "tundra.syntax.glob", "tundra.syntax.embed_lua" },
	Configs = {
		Config { Name = "macosx-clang", Inherit = common, Tools = { "clang-osx" }, DefaultOnHost = "macosx" },
		Config { Name = "macosx-gcc", Inherit = common, Tools = { "gcc-osx" } },
		Config { Name = "linux-gcc", Inherit = common, Tools = { "gcc" }, DefaultOnHost = "linux" },
		Config { Name = "freebsd-gcc", Inherit = common, Tools = { "gcc" }, DefaultOnHost = "freebsd" },
		Config { Name = "openbsd-gcc", Inherit = common, Tools = { "gcc" }, DefaultOnHost = "openbsd" },

		genwincfg("win32-winsdk6", "x86", "9.0"),
		genwincfg("win64-winsdk6", "x86", "9.0"),
		genwincfg("win32-winsdk7", "x86", "10.0"),
		genwincfg("win64-winsdk7", "x86", "10.0"),

		Config {
			Name = "win64-winsdk7",
			Inherit = common,
			Tools = {
				{ "msvc-winsdk"; TargetArch = "x64", VcVersion = "10.0" }
			}
		},

		-- MingW32 cross compilation under OS X
		Config {
			Name = "macosx-mingw32",
			Inherit = common,
			Tools = { "gcc" },
			Env = {
				_GCC_BINPREFIX="i386-mingw32-",
				CCOPTS = "-Werror",
				LUA_EMBED_ASCII = "yes", -- must resort to ascii
			},
			ReplaceEnv = {
				PROGSUFFIX=".exe",
				SHLIBSUFFIX=".dll",
			},
			Virtual = true,
		},

		Config {
			Name = "macosx-crosswin32",
			SubConfigs = {
				host = "macosx-clang",
				target = "macosx-mingw32",
			},
			DefaultSubConfig = "target",
		},
	},
	SubVariants = { "dev", "standalone" },
	DefaultSubVariant = "dev",
}

