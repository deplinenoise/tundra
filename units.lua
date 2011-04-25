
local lua_sources = {
	"lua/src/lapi.c", "lua/src/lauxlib.c", "lua/src/lbaselib.c", "lua/src/lcode.c",
	"lua/src/ldblib.c", "lua/src/ldebug.c", "lua/src/ldo.c", "lua/src/ldump.c",
	"lua/src/lfunc.c", "lua/src/lgc.c", "lua/src/linit.c", "lua/src/liolib.c",
	"lua/src/llex.c", "lua/src/lmathlib.c", "lua/src/lmem.c", "lua/src/loadlib.c",
	"lua/src/lobject.c", "lua/src/lopcodes.c", "lua/src/loslib.c", "lua/src/lparser.c",
	"lua/src/lstate.c", "lua/src/lstring.c", "lua/src/lstrlib.c", "lua/src/ltable.c",
	"lua/src/ltablib.c", "lua/src/ltm.c", "lua/src/lundump.c", "lua/src/lvm.c",
	"lua/src/lzio.c",
}

StaticLibrary {
	Name = "base_lua_host",
	Pass = "CodeGen",
	Sources = lua_sources,
	SubConfig = "host",
}

StaticLibrary {
	Name = "base_lua",
	Pass = "CodeGen",
	Sources = lua_sources,
	SubConfig = "target",
}

Program {
	Name = "gen_lua_data",
	Pass = "CodeGen",
	Target = "$(GEN_LUA_DATA)",
	Config = "*-*-*-standalone",
	Sources = { "src/gen_lua_data.c" },
	SubConfig = "host",
}

Program {
	Name = "luac",
	Pass = "CodeGen",
	Target = "$(LUAC)",
	Config = "*-*-*-standalone",
	Depends = { "base_lua_host" },
	Sources = {
		"lua/src/luac.c",
		"lua/src/print.c",
	},
	SubConfig = "host",
	Libs = { "m", "pthread"; Config = { "linux-*-*", "freebsd-*-*" } },
}

Always "gen_lua_data"
Always "luac"

Program {
	Name = "tundra",
	Defines = { "TD_STANDALONE"; Config = "*-*-*-standalone" },
	Depends = { "base_lua", "luac", "gen_lua_data" },
	Libs = {
		{ "kernel32.lib", "advapi32.lib"; Config = { "win32-*-*", "win64-*-*" } },
		{ "m", "pthread"; Config = { "linux-*-*", "freebsd-*-*" } },
	},
	Sources = {
		"src/ancestors.c",
		"src/build.c",
		"src/build_setup.c",
		"src/bin_alloc.c",
		"src/clean.c",
	 	"src/cpp_scanner.c",
	 	"src/debug.c",
	 	"src/engine.c",
	 	"src/files.c",
		"src/luafs.c",
		"src/md5.c",
		"src/scanner.c",
		"src/tundra.c",
		"src/util.c",
		"src/portable.c",
		"src/relcache.c",
		"src/luaprof.c",

		{
			"src/tty.c",
			"src/exec_unix.c"
			;Config = { "linux-*-*", "freebsd-*-*", "macosx-*-*" }
		},

		{ "src/exec_win32.c"; Config = { "win32-*-*", "win64-*-*" } },

		{
			EmbedLuaSources {
				OutputFile = "data_files.c", Dirs = { "lua/etc", "scripts" }
			}
			; Config = "*-*-*-standalone"
		}
	}
}

Default "tundra"
