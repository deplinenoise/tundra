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

local nodegen = require "tundra.nodegen"

local decl_meta = {}
decl_meta.__index = decl_meta

function make_decl_env()
	local obj = {
		Platforms = {},
		Projects = {},
		ProjectTypes = {},
		SourceGen = {},
		Results = {},
		DefaultNames = {},
		AlwaysNames = {},
	}

	local outer_env = _G
	local function indexfunc(tab, var)
		-- Project types evaluate to functions that just store results
		local p
		p = obj.ProjectTypes[var]
		if p then
			if type(p) == "function" then
				return p
			else
				return function (data)
					obj.Results[#obj.Results + 1] = { Type = var, Decl = data }
				end
			end
		end

		-- Platform names evaluate to themselves
		if obj.Platforms[var] then return var end

		local fn = obj.SourceGen[var]
		if fn then return fn end

		if var == "Default" then
			return function(default_name)
				obj.DefaultNames[#obj.DefaultNames + 1] = default_name
			end
		end

		if var == "Always" then
			return function(always_name)
				obj.AlwaysNames[#obj.AlwaysNames + 1] = always_name
			end
		end

		return outer_env[var]
	end

	obj.FunctionMeta = { __index = indexfunc, __newindex = error }
	obj.FunctionEnv = setmetatable({}, obj.FunctionMeta)

	return setmetatable(obj, decl_meta)
end

function decl_meta:add_platforms(list)
	for i = 1, #list do
		local name = list[i]
		assert(name and type(name) == "string")
		self.Platforms[name] = true
	end
end

function decl_meta:add_project_type(name, fn)
	assert(name and fn)
	self.ProjectTypes[name] = setfenv(fn, self.FunctionEnv)
end

function decl_meta:add_source_generator(name, fn)
	self.SourceGen[name] = fn
end

local function parse_rec(self, unit_generators)
	local chunk
	if type(unit_generators) == "table" then
		for _, gen in ipairs(unit_generators) do
		   parse_rec(self, gen)
		end
		return
	elseif type(unit_generators) == "function" then
		chunk = unit_generators
	elseif type(unit_generators) == "string" then
		chunk = assert(loadfile(unit_generators))
	else
		croak("unknown type %s for unit_generator %q", type(unit_generators), tostring(unit_generators))
	end

	setfenv(chunk, self.FunctionEnv)
	chunk()
end

function decl_meta:parse(unit_generators)
	for name, _ in pairs(nodegen._generator.evaluators) do
		self.ProjectTypes[name] = true
	end

	parse_rec(self, unit_generators)
	
	return self.Results, self.DefaultNames, self.AlwaysNames
end
