
local outer = ...

--tundra.MapPath('src', '.')
--tundra.MapPath('obj', '.')

local env = DefaultEnvironment:Clone()

env:SetMany({
	["RM"] = "del /f /q",
	["RMCOM"] = "$(RM) $(<:b)",
	["C++"] = "cl /nologo",
	["C++FLAGS"] = "/W4",
	["CPPPATH"] = {},
	["CPPDEFINES"] = { "WIN32" },
	["CCCOM"] = "$(C) $(CPPDEFINES:p/D) $(CPPPATH:p/I) $(CFLAGS) /c /Fo$(@:b) $(<:b)",
	["CPPCOM"] = "$(C++) $(CPPDEFINES:p/D) $(CPPPATH:p/I) $(C++FLAGS) /c /Fo$(@:b) $(<:b)",
	["OBJECTSUFFIX"] = ".obj",
	["LIB"] = "lib",
	["LIBSUFFIX"] = ".lib",
	["LIBFLAGS"] = "/nologo",
	["LIBCOM"] = "$(LIB) $(LIBFLAGS) /out:$(@:b) $(LIBPATH:p/libpath\\:) $(<:b)",
})

local lib = env.Make.Library {
	Name = "Lib1",
	Sources = { "a.cpp", "b.cpp" }
}

outer:AddDependency(lib)
