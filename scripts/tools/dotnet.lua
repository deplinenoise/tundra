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

local env, options = ...

load_toolset("generic-dotnet", env)

local setup = toolset_once("dotnet-msvc", function ()
	local frameworkDir = "c:\\Windows\\Microsoft.NET\\Framework"
	local defaultFrameworkVersion = "v3.5"
	return function(env, options)
		local version = options and assert(options.Version) or defaultFrameworkVersion
		env:set_external_env_var('FrameworkDir', frameworkDir)
		env:set_external_env_var('FrameworkVersion', version)
		local binPath = frameworkDir .. "\\" .. version
		env:set_external_env_var('PATH', binPath .. ";" .. env:get_external_env_var('PATH'))
	end
end)

setup(env, options)

-- C# support
env:set_many {
	["CSC"] = "csc.exe",
	["CSPROGSUFFIX"] = ".exe",
	["CSLIBSUFFIX"] = ".dll",
	["CSRESGEN"] = "resgen $(<) $(@)",
	["_CSC_COMMON"] = "-warn:$(CSC_WARNING_LEVEL) /nologo $(CSLIBPATH:b:p/lib\\:) $(CSRESOURCES:b:p/resource\\:) $(CSLIBS:p/reference\\::A.dll)",
	["CSCLIBCOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:library -out:$(@:b) $(<:b)",
	["CSCEXECOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:exe -out:$(@:b) $(<:b)",
}
