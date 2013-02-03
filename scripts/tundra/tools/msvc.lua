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

module(..., package.seeall)

function apply(env, options)

	-- load the generic C toolset first
	tundra.boot.load_toolset("generic-cpp", env)

	-- load support for win32 resource compilation
	tundra.boot.load_toolset("win32-rc", env)

	env:set_many {
		["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".lib", ".obj", ".res", ".rc" },
		["OBJECTSUFFIX"] = ".obj",
		["LIBPREFIX"] = "",
		["LIBSUFFIX"] = ".lib",
		["CC"] = "cl",
		["CXX"] = "cl",
		["LIB"] = "lib",
		["LD"] = "link",
		["CPPDEFS"] = "_WIN32",
		["_CPPDEFS"] = "$(CPPDEFS:p/D) $(CPPDEFS_$(CURRENT_VARIANT:u):p/D)",
		["_USE_PCH_OPT"] = "/Fp$(_PCH_FILE:b) /Yu$(_PCH_HEADER)",
		["_USE_PCH"] = "",
		["_USE_PDB_CC_OPT"] = "/Zi /Fd$(_PDB_FILE:b)",
		["_USE_PDB_LINK_OPT"] = "/DEBUG /PDB:$(_PDB_FILE)",
		["_USE_PDB_CC"] = "",
		["_USE_PDB_LINK"] = "",
		["_USE_MODDEF_OPT"] = "/DEF:$(MODDEF)",
		["_USE_MODDEF"] = "",
		["RC"] = "rc",
		["RCOPTS"] = "/nologo",
		["W32RESSUFFIX"] = ".res",
		["RCCOM"] = "$(RC) $(RCOPTS) /fo$(@:b) $(CPPPATH:b:p/i) $(<:b)",
		["CCCOM"] = "$(CC) /c @RESPONSE|@|$(_CPPDEFS) $(CPPPATH:b:p/I) /nologo $(CCOPTS) $(CCOPTS_$(CURRENT_VARIANT:u)) $(_USE_PCH) $(_USE_PDB_CC) /Fo$(@:b) $(<:b)",
		["CXXCOM"] = "$(CC) /c @RESPONSE|@|$(_CPPDEFS) $(CPPPATH:b:p/I) /nologo $(CXXOPTS) $(CXXOPTS_$(CURRENT_VARIANT:u)) $(_USE_PCH) $(_USE_PDB_CC) /Fo$(@:b) $(<:b)",
		["PCHCOMPILE"] = "$(CC) /c $(_CPPDEFS) $(CPPPATH:b:p/I) /nologo $(CCOPTS) $(CCOPTS_$(CURRENT_VARIANT:u)) /Yc$(_PCH_HEADER) /Fp$(@:[1]:b) /Fo$(@:[2]:b) $(<:[1]:b)",
		["LIBS"] = "",
		["PROGOPTS"] = "",
		["PROGCOM"] = "$(LD) /nologo @RESPONSE|@|$(_USE_PDB_LINK) $(PROGOPTS) $(LIBPATH:b:p/LIBPATH\\:) $(_USE_MODDEF) $(LIBS) /out:$(@:b) $(<:b:p\n)",
		["LIBOPTS"] = "",
		["LIBCOM"] = "$(LIB) /nologo @RESPONSE|@|$(LIBOPTS) /out:$(@:b) $(<:b:p\n)",
		["PROGPREFIX"] = "",
		["SHLIBLINKSUFFIX"] = ".lib",
		["SHLIBPREFIX"] = "",
		["SHLIBOPTS"] = "",
		["SHLIBCOM"] = "$(LD) /DLL /nologo @RESPONSE|@|$(_USE_PDB_LINK) $(SHLIBOPTS) $(LIBPATH:b:p/LIBPATH\\:) $(_USE_MODDEF) $(LIBS) /out:$(@:b) $(<:b)",
		["AUX_FILES_PROGRAM"] = { "$(@:B:a.exe.manifest)", "$(@:B:a.pdb)" },
		["AUX_FILES_SHAREDLIBRARY"] = { "$(@:B:a.dll.manifest)", "$(@:B:a.pdb)", "$(@:B:a.exp)", "$(@:B:a.lib)", },
	}
end
