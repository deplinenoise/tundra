-- Copyright 2013 Andreas Fredriksson
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

-- msvc-vs2012.lua - Settings to use Microsoft Visual Studio 2012 from the
-- registry.

module(..., package.seeall)

local vscommon = require "tundra.tools.msvc-vscommon"

function apply(env, options)
  vscommon.apply_msvc_visual_studio("11.0", env, options)
end
