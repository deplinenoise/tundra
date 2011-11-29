-- Copyright 2011 Andreas Fredriksson
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
	-- load the generic assembly toolset first
	tundra.boot.load_toolset("generic-asm", env)

	env:set_many {
		["YASM"] = "yasm",
		["ASMCOM"] = "$(YASM) -o $(@) $(ASMDEFS:p-D ) $(ASMOPTS) $(<)",
		["ASMINC_KEYWORDS"] = { "%include" },
	}
end
