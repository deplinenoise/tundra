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

-- tundra.lua - Self-hosting build file for Tundra itself

local common = {
	Env = { CPPPATH = { "src", "lua/src" } },
}

Build {
	Units = "units.lua",
	Configs = {
		{ Name = "macosx-clang", Inherit = common, Tools = { "clang-osx" }, DefaultOnHost = "macosx" },
		{ Name = "macosx-gcc", Inherit = common, Tools = { "gcc-osx" } },
		{ Name = "win32-msvc", Inherit = common, Tools = { { "msvc-vs2008"; TargetArch = "x86"} } },
		{ Name = "win64-msvc", Inherit = common, Tools = { { "msvc-vs2008"; TargetArch = "x64"} } },
		{ Name = "linux-gcc", Inherit = common, Tools = { "gcc" } },
	},
}

