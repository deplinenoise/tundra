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

load_toolset("generic-dotnet", env)

env:set_many {
	["CSC"] = "gmcs",
	["CSPROGSUFFIX"] = ".exe",
	["CSLIBSUFFIX"] = ".dll",
	["CSRESGEN"] = "resgen2 $(<) $(@)",
	["_CSC_COMMON"] = "-warn:$(CSC_WARNING_LEVEL) /nologo $(CSLIBPATH:p-lib\\:) $(CSRESOURCES:p-resource\\:) $(CSLIBS:p-reference\\::A.dll)",
	["CSCLIBCOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:library -out:$(@) $(<)",
	["CSCEXECOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:exe -out:$(@) $(<)",
}
