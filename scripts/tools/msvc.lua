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

local env = ...

-- load the generic C toolset first
load_toolset("generic-cpp", env)

env:set_many {
	["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".lib", ".obj" },
	["OBJECTSUFFIX"] = ".obj",
	["LIBSUFFIX"] = ".lib",
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
	["SHLIBCOM"] = "$(LD) /DLL /nologo @RESPONSE|@|$(SHLIBOPTS) $(LIBPATH:b:p/LIBPATH\\:) $(LIBS) /out:$(@:b) $(<:b)",
	["AUX_FILES_PROGRAM"] = { "$(@:B:a.exe.manifest)", "$(@:B:a.pdb)" },
	["AUX_FILES_SHAREDLIB"] = { "$(@:B:a.dll.manifest)", "$(@:B:a.pdb)", "$(@:B:a.exp)", "$(@:B:a.lib)", },
}

