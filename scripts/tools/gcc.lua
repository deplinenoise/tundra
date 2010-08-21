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

local native = require("tundra.native")

env:set_many {
	["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".a", ".o" },
	["OBJECTSUFFIX"] = ".o",
	["LIBSUFFIX"] = ".a",
	["_GCC_BINPREFIX"] = "",
	["CC"] = "$(_GCC_BINPREFIX)gcc",
	["C++"] = "$(_GCC_BINPREFIX)g++",
	["LIB"] = "$(_GCC_BINPREFIX)ar",
	["LD"] = "$(_GCC_BINPREFIX)gcc",
	["_OS_CCOPTS"] = "",
	["CCOPTS"] = "-Wall",
	["CCCOM"] = "$(CC) $(_OS_CCOPTS) -c $(CPPDEFS:p-D) $(CPPPATH:f:p-I) $(CCOPTS) $(CCOPTS_$(CURRENT_VARIANT:u)) -o $(@) $(<)",
	["CXXCOM"] = "$(CCCOM)",
	["PROGOPTS"] = "",
	["PROGCOM"] = "$(LD) $(PROGOPTS) $(LIBS:p-l) -o $(@) $(<)",
	["LIBOPTS"] = "",
	["LIBCOM"] = "$(LIB) -r $(LIBOPTS) $(@) $(<)",
}
