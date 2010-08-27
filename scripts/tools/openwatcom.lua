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

-- openwatcom.lua - Support for the Open Watcom compiler C/C++ compiler

module(..., package.seeall)

local native = require "tundra.native"
local os = require "os"

local function setup(env, options)

	if native.host_platform ~= "windows" then
		error("the openwatcom toolset only works on windows hosts")
	end

	assert(options, "No Options provided")
	local dir = assert(options.InstallDir)
	env:set_external_env_var("WATCOM", dir)
	env:set_external_env_var("EDPATH", dir .. "\\EDDAT")
	env:set_external_env_var("WIPFC", dir .. "\\WIPFC")
	local p = native.getenv("PATH") .. ";" .. dir .. "\\BINNT\\;" .. dir .. "\\BINW\\"
	print(p)
	env:set_external_env_var("PATH", p)
	local inc = native.getenv("INCLUDE", "")
	if inc then
		inc = inc .. ";"
	end
	env:set_external_env_var("INCLUDE", inc .. dir .. "\\H;" .. dir .. "\\H\\NT;" .. dir .. "\\H\\NT\\DIRECTX;" .. dir .. "\\H\\NT\\DDK")

end

function(env, options)
	-- load the generic C toolset first
	load_toolset("generic-cpp", env)

	setup(env, options)

	env:set_many {
		["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".lib", ".obj" },
		["OBJECTSUFFIX"] = ".obj",
		["LIBSUFFIX"] = ".lib",
		["CC"] = "wcl386.exe",
		["C++"] = "wcl386.exe",
		["LIB"] = "wlib.exe",
		["LD"] = "wlink.exe",
		["CPPDEFS"] = "_WIN32",
		["CCOPTS"] = "-wx -we",
		["_CPPDEFS"] = "$(CPPDEFS:p-d) $(CPPDEFS_$(CURRENT_VARIANT:u):p-d)",
		["_USE_PCH_OPT"] = "",
		["_USE_PCH"] = "",
		["_CCCOM"] = "$(CC) /c @RESPONSE|@|$(_CPPDEFS) $(CPPPATH:b:p-i) $(CCOPTS) $(CCOPTS_$(CURRENT_VARIANT:u)) $(_USE_PCH) -fo=$(@:b) $(<:b)",
		["CCCOM"] = "$(_CCCOM)",
		["CXXCOM"] = "$(_CCCOM)",
		["PCHCOMPILE"] = "",
		["LIBS"] = "",
		["PROGOPTS"] = "",
		["PROGCOM"] = "", -- "$(LD) @RESPONSE|@|$(PROGOPTS) $(LIBS) /out:$(@:b) $(<:b)",
		["LIBOPTS"] = "",
		["LIBCOM"] = "", -- "$(LIB) @RESPONSE|@|$(LIBOPTS) /out:$(@:b) $(<:b)",
		["SHLIBOPTS"] = "",
		["SHLIBCOM"] = "", -- "$(LD) /nologo @RESPONSE|@|$(SHLIBOPTS) $(LIBPATH:b:p/LIBPATH\\:) $(LIBS) /out:$(@:b) $(<:b)",
	}

