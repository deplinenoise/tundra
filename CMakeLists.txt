cmake_minimum_required(VERSION 2.6)

project(Tundra)

include_directories(src lua/src)

if (WIN32)
    set(CMAKE_C_FLAGS "/EHsc")

    # Warning options--warning level 4 and warnings as errors
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /WX")
    
    # disable warning 4127: Conditional expression is constant
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4127")

    # disable warning 4505: unreferenced local function has been removed
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4505")

    # disable warning 4275: non dll-interface class 'Foo' used as base for dll-interface class 'Bar'
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4275")

    # disable warning 4100: unreferenced formal parameter
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4100")

    # disable warning 4251: class 'Bar' needs to have dll-interface to be used by clients of struct 'Foo'
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4251")
    
    # disable warning 4324: 'Foo' : structure was padded due to __declspec(align()) 
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4324")
    
    # disable warning 4718: 'Foo' : recursive call has no side effects, deleting
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4718")
    
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /we4062 /we4263 /we4431")

    # Stick debug into in the object files rather in the PDB file while
    # compiling. Enables parallel builds and prevents msbdpsrv.exe from
    # crashing all the time with msvc 9.
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Z7")

    # General purpose Win32 defines
    add_definitions(/DWIN32 /D_WIN32 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS)
endif (WIN32)

if (UNIX)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Werror")
endif (UNIX)

add_executable(gen_lua_data src/gen_lua_data.c)

set(LUA_SOURCES 
	lua/src/lapi.c lua/src/lauxlib.c lua/src/lbaselib.c lua/src/lcode.c
	lua/src/ldblib.c lua/src/ldebug.c lua/src/ldo.c lua/src/ldump.c
	lua/src/lfunc.c lua/src/lgc.c lua/src/linit.c lua/src/liolib.c
	lua/src/llex.c lua/src/lmathlib.c lua/src/lmem.c lua/src/loadlib.c
	lua/src/lobject.c lua/src/lopcodes.c lua/src/loslib.c lua/src/lparser.c
	lua/src/lstate.c lua/src/lstring.c lua/src/lstrlib.c lua/src/ltable.c
	lua/src/ltablib.c lua/src/ltm.c lua/src/lundump.c lua/src/lvm.c
	lua/src/lzio.c lua/src/lapi.h lua/src/lauxlib.h lua/src/lcode.h
	lua/src/ldebug.h lua/src/ldo.h lua/src/lfunc.h lua/src/lgc.h lua/src/llex.h
	lua/src/llimits.h lua/src/lmem.h lua/src/lobject.h lua/src/lopcodes.h
	lua/src/lparser.h lua/src/lstate.h lua/src/lstring.h lua/src/ltable.h
	lua/src/ltm.h lua/src/lua.h lua/src/luaconf.h lua/src/lualib.h
	lua/src/lundump.h lua/src/lvm.h lua/src/lzio.h
	)

source_group(Lua FILES ${LUA_SOURCES})

set(TUNDRA_SOURCES
src/ancestors.c
src/ancestors.h
src/bin_alloc.c
src/bin_alloc.h
src/build.c
src/build.h
src/build_setup.c
src/clean.c
src/clean.h
src/cpp_scanner.c
src/debug.c
src/debug.h
src/engine.c
src/engine.h
src/files.c
src/files.h
src/luafs.c
src/md5.h
src/md5.c
src/scanner.c
src/scanner.h
src/tundra.c
src/util.c
src/util.h
src/portable.c
src/portable.h
src/relcache.c
src/relcache.h
src/luaprof.c
	)

source_group(Tundra FILES ${TUNDRA_SOURCES})

if (UNIX)
	link_libraries(tundra m pthread)
endif (UNIX)

add_executable(tundra ${TUNDRA_SOURCES} ${LUA_SOURCES})

