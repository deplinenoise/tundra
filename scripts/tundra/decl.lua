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

local functions = {}
local _decl_meta = {}
_decl_meta.__index = _decl_meta

local current = nil

local function new_parser()
	local obj = {
		Functions = {},
		Results = {},
		DefaultTargets = {},
		AlwaysTargets = {},
	}

	local outer_env = _G
	local iseval = nodegen.is_evaluator
	local function indexfunc(tab, var)
		if iseval(var) then
			-- Return an anonymous function such that
			-- the code "Foo { ... }" will result in a call to
			-- "nodegen.evaluate('Foo', { ... })"
			return function (data)
				local result = nodegen.evaluate(var, data)
				obj.Results[#obj.Results + 1] = result
				return result
			end
		end
		local p = obj.Functions[var]
		if p then return p end
		return outer_env[var]
	end

	obj.FunctionMeta = { __index = indexfunc, __newindex = error }
	obj.FunctionEnv = setmetatable({}, obj.FunctionMeta)

	for name, fn in pairs(functions) do
		obj.Functions[name] = setfenv(fn, obj.FunctionEnv)
	end

	obj.Functions["Default"] = function(default_obj)
		obj.DefaultTargets[#obj.DefaultTargets + 1] = default_obj
	end

	obj.Functions["Always"] = function(always_obj)
		obj.AlwaysTargets[#obj.AlwaysTargets + 1] = always_obj
	end

	current = setmetatable(obj, _decl_meta)
	return current
end

function add_function(name, fn)
	assert(name and fn)
	functions[name] = fn

	if current then
		-- require called from within unit script
		current.Functions[name] = setfenv(fn, current.FunctionEnv)
	end
end

function _decl_meta:parse_rec(data)
	local chunk
	if type(data) == "table" then
		for _, gen in ipairs(data) do
		   self:parse_rec(gen)
		end
		return
	elseif type(data) == "function" then
		chunk = data
	elseif type(data) == "string" then
		chunk = assert(loadfile(data))
	else
		croak("unknown type %s for unit_generator %q", type(data), tostring(data))
	end

	setfenv(chunk, self.FunctionEnv)
	chunk()
end

function parse(data)
	p = new_parser()
	current = p
	p:parse_rec(data)
	current = nil
	return p.Results, p.DefaultTargets, p.AlwaysTargets
end
