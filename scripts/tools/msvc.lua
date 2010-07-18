local env = ...

-- load the generic C toolset first
load_toolset("generic-cpp", env)

local native = require("tundra.native")
--local vs9_key = "SOFTWARE\\Microsoft\\VisualStudio\\9.0"

assert(native.host_platform == "windows", "the msvc toolset only works on windows hosts")

--local path = assert(native.reg_query("HKLM", vs9_key, "InstallDir"))
--path = string.gsub(path, "\\Common7\\IDE\\$", "")
--local cl_exe = '"' .. path .. "\\vc\\bin\\cl.exe" ..'"'
--local lib_exe = '"' .. path .. "\\vc\\bin\\lib.exe" ..'"'
--local link_exe = '"' .. path .. "\\vc\\bin\\link.exe" ..'"'

env:set_many {
	["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".lib", ".obj" },
	["OBJECTSUFFIX"] = ".obj",
	["PROGSUFFIX"] = ".exe",
	["LIBSUFFIX"] = ".lib",
	["CC"] = "cl",
	["C++"] = "cl",
	["LIB"] = "lib",
	["LD"] = "link",
	["CPPDEFS"] = "_WIN32",
	["CCOPTS"] = "/W4",
	["CCCOM"] = "$(CC) /c $(CPPDEFS:p/D) $(CPPPATH:b:p/I) /nologo $(CCOPTS) $(CCOPTS_$(CURRENT_CONFIG:u)) /Fo$(@:b) $(<:b)",
	["CXXCOM"] = "$(CC) /c $(CPPDEFS:p/D) $(CPPPATH:b:p/I) /nologo $(CCOPTS) $(CCOPTS_$(CURRENT_CONFIG:u)) /Fo$(@:b) $(<:b)",
	["LIBS"] = "",
	["PROGOPTS"] = "",
	["PROGCOM"] = "$(LD) /nologo $(PROGOPTS) $(LIBS) /out:$(@:b) $(<:b)",
	["LIBOPTS"] = "",
	["LIBCOM"] = "$(LIB) /nologo $(LIBOPTS) /out:$(@:b) $(<:b)",
}

