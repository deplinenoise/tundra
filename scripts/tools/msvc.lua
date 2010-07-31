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
	["SHLIBSUFFIX"] = ".dll",
	["CC"] = "cl",
	["C++"] = "cl",
	["LIB"] = "lib",
	["LD"] = "link",
	["CPPDEFS"] = "_WIN32",
	["CCOPTS"] = "/W4",
	["_CPPDEFS"] = "$(CPPDEFS:p/D) $(CPPDEFS_$(CURRENT_VARIANT:u):p/D)",
	["_USE_PCH_OPT"] = "/Fp$(_PCH_FILE:b) /Yu$(_PCH_HEADER)",
	["_USE_PCH"] = "",
	["CCCOM"] = "$(CC) /c @RESPONSE|@|$(_CPPDEFS) $(CPPPATH:b:p/I) /nologo $(CCOPTS) $(CCOPTS_$(CURRENT_VARIANT:u)) $(_USE_PCH) /Fo$(@:b) $(<:b)",
	["CXXCOM"] = "$(CC) /c @RESPONSE|@|$(_CPPDEFS) $(CPPPATH:b:p/I) /nologo $(CCOPTS) $(CCOPTS_$(CURRENT_VARIANT:u)) $(_USE_PCH) /Fo$(@:b) $(<:b)",
	["PCHCOMPILE"] = "$(CC) /c $(_CPPDEFS) $(CPPPATH:b:p/I) /nologo $(CCOPTS) $(CCOPTS_$(CURRENT_VARIANT:u)) /Yc$(_PCH_HEADER) /Fp$(@:b) $(<:[1]:b)",
	["LIBS"] = "",
	["PROGOPTS"] = "",
	["PROGCOM"] = "$(LD) /nologo @RESPONSE|@|$(PROGOPTS) $(LIBS) /out:$(@:b) $(<:b)",
	["LIBOPTS"] = "",
	["LIBCOM"] = "$(LIB) /nologo @RESPONSE|@|$(LIBOPTS) /out:$(@:b) $(<:b)",
	["SHLIBOPTS"] = "",
	["SHLIBCOM"] = "$(LD) /DLL /nologo @RESPONSE|@|$(SHLIBOPTS) $(LIBPATH:b:p/LIBPATH\\:) /out:$(@:b) $(<:b)",
}

