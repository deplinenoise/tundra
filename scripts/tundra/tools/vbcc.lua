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
	-- Also add assembly support.
	tundra.boot.load_toolset("generic-asm", env)

	local vbcc_root = assert(native.getenv("VBCC"), "VBCC environment variable must be set")

	env:set_many {
		["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".s", ".asm", ".a", ".o" },
		["OBJECTSUFFIX"] = ".o",
		["LIBPREFIX"] = "",
		["LIBSUFFIX"] = ".a",
		["VBCC_ROOT"] = vbcc_root,
		["CC"] = vbcc_root .. "$(SEP)bin$(SEP)vc$(HOSTPROGSUFFIX)",
		["LIB"] = vbcc_root .. "$(SEP)bin$(SEP)vlink$(HOSTPROGSUFFIX)",
		["LD"] = vbcc_root .. "$(SEP)bin$(SEP)vc$(HOSTPROGSUFFIX)",
		["ASM"] = vbcc_root .. "$(SEP)bin$(SEP)vasmm68k_mot$(HOSTPROGSUFFIX)",
		["VBCC_SDK_INC"] = vbcc_root .. "$(SEP)include$(SEP)sdk",
		["_OS_CCOPTS"] = "",
		["_OS_CXXOPTS"] = "",
		["CCCOM"] = "$(CC) $(_OS_CCOPTS) -c $(CPPDEFS:p-D) $(CPPPATH:f:p-I) $(CCOPTS) $(CCOPTS_$(CURRENT_VARIANT:u)) -o $(@) $(<)",
		["ASMCOM"] = "$(ASM) -quiet -Fhunk -phxass $(ASMOPTS) $(ASMOPTS_$(CURRENT_VARIANT:u)) $(ASMDEFS:p-D) $(ASMINCPATH:f:p-I) -I$(VBCC_SDK_INC) -o $(@) $(<)",
		["PROGOPTS"] = "",
		["PROGCOM"] = "$(LD) $(PROGOPTS) $(LIBPATH:p-L) $(LIBS:p-l) -o $(@) $(<)",
		["PROGPREFIX"] = "",
		["LIBOPTS"] = "",
		["LIBCOM"] = "$(LIB) -r $(LIBOPTS) -o $(@) $(<)",
		["ASMINC_KEYWORDS"] = { "INCLUDE", "include" },
		["ASMINC_BINARY_KEYWORDS"] = { "INCBIN", "incbin" },
	}
end
