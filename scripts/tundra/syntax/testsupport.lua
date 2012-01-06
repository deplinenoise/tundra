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

-- testsupport.lua: A simple UpperCaseFile unit used for Tundra's test harness

module(..., package.seeall)

local util = require 'tundra.util'
local nodegen = require 'tundra.nodegen'

local mt = nodegen.create_eval_subclass {}

function mt:create_dag(env, data, deps)
	return env:make_node {
		Pass = data.Pass,
		Label = "UpperCaseFile \$(@)",
		Action = "tr a-z A-Z < \$(<) > \$(@)",
		InputFiles = { data.InputFile },
		OutputFiles = { data.OutputFile },
		Dependencies = deps,
	}
end

nodegen.add_evaluator("UpperCaseFile", mt, {
	Name = { Type = "string", Required = "true" },
	InputFile = { Type = "string", Required = "true" },
	OutputFile = { Type = "string", Required = "true" },
})


