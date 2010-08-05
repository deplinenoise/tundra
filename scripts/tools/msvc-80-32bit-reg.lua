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

-- msvc-80-32bit-reg.lua - Settings to use Microsoft Visual Studio 2008 from
-- the registry in 32-bit mode

local env = ...

-- Load basic MSVC environment setup first. We're going to replace the paths to
-- some tools.
load_toolset('msvc', env)

local msvc_setup = require "tundra.toolsupport.msvc"

msvc_setup.setup(env, 'x86')
