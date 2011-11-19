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

local native = require "tundra.native"

function apply(env, options)
	-- load the generic C toolset first
	tundra.boot.load_toolset("generic-cpp", env)

	env:set_many {
		["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".a", ".o" },
		["OBJECTSUFFIX"] = ".o",
		["LIBPREFIX"] = "lib",
		["LIBSUFFIX"] = ".a",
		["_GCC_BINPREFIX"] = "",
		["CC"] = "$(_GCC_BINPREFIX)gcc",
		["CXX"] = "$(_GCC_BINPREFIX)g++",
		["LIB"] = "$(_GCC_BINPREFIX)ar",
		["LD"] = "$(_GCC_BINPREFIX)gcc",
		["_OS_CCOPTS"] = "",
		["_OS_CXXOPTS"] = "",
		["CCCOM"] = "$(CC) $(_OS_CCOPTS) -c $(CPPDEFS:p-D) $(CPPPATH:f:p-I) $(CCOPTS) $(CCOPTS_$(CURRENT_VARIANT:u)) -o $(@) $(<)",
		["CXXCOM"] = "$(CXX) $(_OS_CXXOPTS) -c $(CPPDEFS:p-D) $(CPPPATH:f:p-I) $(CXXOPTS) $(CXXOPTS_$(CURRENT_VARIANT:u)) -o $(@) $(<)",
		["PROGOPTS"] = "",
		["PROGCOM"] = "$(LD) $(PROGOPTS) $(LIBPATH:p-L) -o $(@) $(<) $(LIBS:p-l)",
		["PROGPREFIX"] = "",
		["LIBOPTS"] = "",
		["LIBCOM"] = "$(LIB) -rs $(LIBOPTS) $(@) $(<)",
		["SHLIBPREFIX"] = "lib",
		["SHLIBOPTS"] = "-shared",
		["SHLIBCOM"] = "$(LD) $(SHLIBOPTS) $(LIBPATH:p-L) -o $(@) $(<) $(LIBS:p-l)",
	}
end
