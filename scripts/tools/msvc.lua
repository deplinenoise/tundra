local env = ...

-- load the generic C toolset first
load_toolset("generic-cpp", env)

local native = require("tundra.native")

local vs9_key = "SOFTWARE\\Microsoft\\VisualStudio\\9.0"

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
	["CCCOM"] = "$(CC) /c $(CPPDEFS:p/D) $(CPPPATH:b:p/I) /nologo $(CCOPTS) /Fo$(@:b) $(<:b)",
	["CXXCOM"] = "$(CC) /c $(CPPDEFS:p/D) $(CPPPATH:b:p/I) /nologo $(CCOPTS) /Fo$(@:b) $(<:b)",
	["LIBS"] = "",
	["PROGOPTS"] = "",
	["PROGCOM"] = "$(LD) /nologo $(PROGOPTS) $(LIBS) /out:$(@:b) $(<:b)",
	["LIBOPTS"] = "",
	["LIBCOM"] = "$(LIB) /nologo $(LIBOPTS) /out:$(@:b) $(<:b)",
}

-- C# support
env:set_many {
	["CSC"] = "csc",
	["CSPROGSUFFIX"] = ".exe",
	["CSLIBSUFFIX"] = ".dll",
	["CSLIBS"] = "",
	["CSRESOURCES"] = "",
	["CSRESGEN"] = "resgen $(<) $(@)",
	["CSC_WARNING_LEVEL"] = "4",
	["_CSC_COMMON"] = "-warn:$(CSC_WARNING_LEVEL) /nologo $(CSLIBPATH:b:p/lib\\:) $(CSRESOURCES:b:p/resource\\:) $(CSLIBS:p/reference\\::A.dll)",
	["CSCOPTS"] = "-optimize",
	["CSCLIBCOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:library -out:$(@:b) $(<:b)",
	["CSCEXECOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:exe -out:$(@:b) $(<:b)",
}
