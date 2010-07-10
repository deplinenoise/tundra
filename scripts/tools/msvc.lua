local env = ...

-- load the generic C toolset first
load_toolset("generic-cpp", env)

local native = require("tundra.native")

local vs9_key = "SOFTWARE\\Microsoft\\VisualStudio\\9.0"

assert(native.host_platform == "windows", "the msvc toolset only works on windows hosts")

local path = assert(native.reg_query("HKLM", vs9_key, "InstallDir"))

path = string.gsub(path, "\\Common7\\IDE\\$", "")

local cl_exe = '"' .. path .. "\\vc\\bin\\cl.exe" ..'"'
local lib_exe = '"' .. path .. "\\vc\\bin\\lib.exe" ..'"'
local link_exe = '"' .. path .. "\\vc\\bin\\link.exe" ..'"'

env:set_many {
	["OBJECTSUFFIX"] = ".obj",
	["PROGSUFFIX"] = ".exe",
	["LIBSUFFIX"] = ".lib",
	["CC"] = cl_exe,
	["C++"] = cl_exe,
	["LIB"] = lib_exe,
	["LD"] = link_exe,
	["CPPDEFS"] = "_WIN32",
	["CCOPTS"] = "/W3",
	["CCCOM"] = "$(CC) /c $(CPPDEFS:p/D) $(CPPPATH:b:p/I) /nologo $(CCOPTS) /Fo$(@:b) $(<:b)",
	["CSC"] = "csc.exe",
	["CSCEXECOM"] = "$(CSC) /nologo /target:exe /warn:4 /optimize /out:$(@:b) $(<:b)",
	["PROGLIBS"] = "",
	["PROGOPTS"] = "",
	["PROGCOM"] = "$(LD) /machine:x86 /nologo $(PROGOPTS) $(PROGLIBS) /out:$(@:b) $(<:b)",
	["LIBOPTS"] = "",
	["LIBCOM"] = "$(LIB) /nologo $(LIBOPTS) /out:$(@:b) $(<:b)",
}

