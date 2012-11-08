-- Copyright 2010-2012 Andreas Fredriksson
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

	-- load the generic GCC toolset first
	tundra.boot.load_toolset("gcc", env)

	-- load support for win32 resource compilation
	tundra.boot.load_toolset("win32-rc", env)

	env:set_many {
		["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".a", ".o", ".rc" },
		["OBJECTSUFFIX"] = ".o",
		["LIBPREFIX"] = "",
		["LIBSUFFIX"] = ".a",
		["W32RESSUFFIX"] = ".o",
		["CPPDEFS"] = "_WIN32",
		["_CPPDEFS"] = "$(CPPDEFS:p/D) $(CPPDEFS_$(CURRENT_VARIANT:u):p/D)",
		["RC"] = "windres",
		["RCOPTS"] = "",
		["RCCOM"] = "$(RC) $(RCOPTS) --output=$(@:b) $(CPPPATH:b:p-I) --input=$(<:b)",
		["SHLIBLINKSUFFIX"] = ".a",
	}
end
