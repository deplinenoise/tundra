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

local _outer_env = ...
local depgraph = require("tundra.depgraph")
local util = require("tundra.util")
local path = require("tundra.path")
local native = require("tundra.native")

local function get_cpp_scanner(env, fn)
	local function new_scanner()
		local paths = util.map(env:get_list("CPPPATH"), function (v) return env:interpolate(v) end)
		return GlobalEngine:make_cpp_scanner(paths)
	end
	return env:memoize("CPPPATH", "_cpp_scanner", new_scanner)
end

do
	local _anyc_compile = function(env, pass, fn, label, action)
		local pch_input = env:get('_PCH_FILE', '')
		local function obj_fn()
			if fn:match('^%$%(OBJECTDIR%)') then
				return path.drop_suffix(fn) .. '$(OBJECTSUFFIX)'
			else
				return '$(OBJECTDIR)/' .. path.drop_suffix(fn) .. '$(OBJECTSUFFIX)'
			end
		end
		local object_fn = obj_fn(fn)
		local implicit_inputs = nil
		if pch_input ~= '' then
			implicit_inputs = { pch_input }
		end

		return env:make_node {
			Label = label .. ' $(@)',
			Pass = pass,
			Action = action,
			InputFiles = { fn },
			OutputFiles = { object_fn },
			ImplicitInputs = implicit_inputs,
			Scanner = get_cpp_scanner(env, fn),
		}
	end

	local cc_compile = function(env, pass, fn)
		return _anyc_compile(env, pass, fn, "Cc", "$(CCCOM)")
	end

	local objc_compile = function(env, pass, fn)
		return _anyc_compile(env, pass, fn, "ObjC", "$(OBJCCOM)")
	end

	local cxx_compile = function(env, pass, fn)
		return _anyc_compile(env, pass, fn, "C++", "$(CXXCOM)")
	end

	_outer_env:register_implicit_make_fn("c", cc_compile)
	_outer_env:register_implicit_make_fn("m", objc_compile)
	_outer_env:register_implicit_make_fn("cpp", cxx_compile)
	_outer_env:register_implicit_make_fn("cc", cxx_compile)
	_outer_env:register_implicit_make_fn("cxx", cxx_compile)
end

_outer_env:set_many {
	["HEADERS_EXTS"] = { ".h", ".hpp", ".hh", ".hxx", ".inl" },
	["CPPPATH"] = "",
	["CPPDEFS"] = "",
	["LIBS"] = "",
	["LIBPATH"] = "",
	["CPPDEFS_DEBUG"] = "",
	["CPPDEFS_PRODUCTION"] = "",
	["CPPDEFS_RELEASE"] = "",
	["CCOPTS_DEBUG"] = "",
	["CCOPTS_PRODUCTION"] = "",
	["CCOPTS_RELEASE"] = "",
}
