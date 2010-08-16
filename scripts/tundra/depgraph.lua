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

local util = require("tundra.util")
local path = require("tundra.path")
local native = require("tundra.native")
local environment = require("tundra.environment")

local default_pass = { Name = "Default", BuildOrder = 100000 }

function create_node(env_, data_)
	assert(environment.is_environment(env_))

	local function normalize_paths(paths)
		return util.mapnil(paths, function (x)
			if type(x) == "string" then
				return path.normalize(env_:interpolate(x))
			else
				return x
			end
		end)
	end

	-- these are the inputs that $(<) expand to
	local regular_inputs = normalize_paths(data_.InputFiles)

	-- these are other, auxillary input files that shouldn't appear on the command line
	-- useful to e.g. add an input dependency on a tool
	local implicit_inputs = normalize_paths(data_.ImplicitInputs)

	local inputs = util.merge_arrays_2(regular_inputs, implicit_inputs)
	local outputs = normalize_paths(data_.OutputFiles)
	
	local expand_env = {
		['<'] = normalize_paths(regular_inputs),
		['@'] = outputs
	}

	local params = {
		pass = data_.Pass or default_pass,
		salt = env_:get("BUILD_ID", ""),
		scanner = data_.Scanner,
		deps = data_.Dependencies,
		inputs = inputs,
		outputs = outputs,
		aux_outputs = util.mapnil(data_.AuxOutputFiles, function (x)
			local result = env_:interpolate(x, expand_env)
			return path.normalize(result)
		end),
		env = env_.external_vars,
	}

	if data_.Action then
		params.action = env_:interpolate(data_.Action, expand_env)
	else
		assert(not params.outputs, "can't have output files without an action")
		params.action = ""
	end

	params.annotation = env_:interpolate(data_.Label or "?", expand_env)

	return GlobalEngine:make_node(params)
end

