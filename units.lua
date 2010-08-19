Program {
	Name = "tundra",
	Sources = {
		"lua/src/lapi.c", "lua/src/lauxlib.c", "lua/src/lbaselib.c", "lua/src/lcode.c",
		"lua/src/ldblib.c", "lua/src/ldebug.c", "lua/src/ldo.c", "lua/src/ldump.c",
		"lua/src/lfunc.c", "lua/src/lgc.c", "lua/src/linit.c", "lua/src/liolib.c",
		"lua/src/llex.c", "lua/src/lmathlib.c", "lua/src/lmem.c", "lua/src/loadlib.c",
		"lua/src/lobject.c", "lua/src/lopcodes.c", "lua/src/loslib.c", "lua/src/lparser.c",
		"lua/src/lstate.c", "lua/src/lstring.c", "lua/src/lstrlib.c", "lua/src/ltable.c",
		"lua/src/ltablib.c", "lua/src/ltm.c", "lua/src/lundump.c", "lua/src/lvm.c",
		"lua/src/lzio.c",

		"src/bin_alloc.c", "src/build.c",
		"src/cpp_scanner.c", "src/debug.c", "src/engine.c",
		"src/luafs.c", "src/md5.c", "src/scanner.c",
		"src/tundra.c", "src/util.c", "src/portable.c",
		"src/relcache.c", "src/luaprof.c",
	}
}

Default "tundra"
