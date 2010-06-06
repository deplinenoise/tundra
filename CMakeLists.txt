cmake_minimum_required(VERSION 2.6)

project(Tundra)

include_directories(src lua/src)

if (WIN32)
    set(CMAKE_CXX_FLAGS "/EHsc")

    # Warning options--warning level 4 and warnings as errors
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
    
    # disable warning 4127: Conditional expression is constant
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4127")

    # disable warning 4505: unreferenced local function has been removed
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4505")

    # disable warning 4275: non dll-interface class 'Foo' used as base for dll-interface class 'Bar'
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4275")

    # disable warning 4100: unreferenced formal parameter
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100")

    # disable warning 4251: class 'Bar' needs to have dll-interface to be used by clients of struct 'Foo'
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4251")
    
    # disable warning 4324: 'Foo' : structure was padded due to __declspec(align()) 
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4324")
    
    # disable warning 4718: 'Foo' : recursive call has no side effects, deleting
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4718")
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4062 /we4263 /we4431")

    # Stick debug into in the object files rather in the PDB file while
    # compiling. Enables parallel builds and prevents msbdpsrv.exe from
    # crashing all the time with msvc 9.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Z7")

    # General purpose Win32 defines
    add_definitions(/DWIN32 /D_WIN32 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS)
endif (WIN32)

set(LUA_SOURCES 
	lua/src/lapi.c
	lua/src/lauxlib.c
	lua/src/lbaselib.c
	lua/src/lcode.c
	lua/src/ldblib.c
	lua/src/ldebug.c
	lua/src/ldo.c
	lua/src/ldump.c
	lua/src/lfunc.c
	lua/src/lgc.c
	lua/src/linit.c
	lua/src/liolib.c
	lua/src/llex.c
	lua/src/lmathlib.c
	lua/src/lmem.c
	lua/src/loadlib.c
	lua/src/lobject.c
	lua/src/lopcodes.c
	lua/src/loslib.c
	lua/src/lparser.c
	lua/src/lstate.c
	lua/src/lstring.c
	lua/src/lstrlib.c
	lua/src/ltable.c
	lua/src/ltablib.c
	lua/src/ltm.c
	lua/src/lundump.c
	lua/src/lvm.c
	lua/src/lzio.c
	)

file(GLOB TUNDRA_SOURCES src/*.c src/*.h)

add_executable(tundra ${TUNDRA_SOURCES} ${LUA_SOURCES})
