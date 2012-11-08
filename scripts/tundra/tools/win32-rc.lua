-- Copyright 2010-2012 Andreas Fredriksson
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

local path = require("tundra.path")
local gencpp = require("tundra.tools.generic-cpp")

local function compile_resource_file(env, pass, fn)
	return env:make_node {
		Label = 'Rc $(@)',
		Pass = pass,
		Action = "$(RCCOM)",
		InputFiles = { fn },
		OutputFiles = { path.make_object_filename(env, fn, env:get('W32RESSUFFIX')) },
		Scanner = gencpp.get_cpp_scanner(env, fn),
	}
end

function apply(env, options)
	env:register_implicit_make_fn("rc", compile_resource_file)
end
