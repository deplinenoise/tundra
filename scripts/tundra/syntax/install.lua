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

-- install.lua - Express file copying in unit form.

module(..., package.seeall)

local nodegen = require "tundra.nodegen"
local files = require "tundra.syntax.files"
local path = require "tundra.path"
local util = require "tundra.util"

local _mt = nodegen.create_eval_subclass {}

local blueprint = {
	Sources = { Type = "source_list", Required = true },
	TargetDir = { Type = "string", Required = true },
}

function _mt:create_dag(env, data, deps)
	local my_pass = data.Pass
	local sources = data.Sources
	local target_dir = data.TargetDir

	local copies = {}

	-- all the copy operations will depend on all the incoming deps
	for _, src in util.nil_ipairs(sources) do
		local base_fn = select(2, path.split(src))
		local target = target_dir .. '/' .. base_fn
		copies[#copies + 1] = files.copy_file(env, src, target, my_pass, deps)
	end

	return env:make_node {
		Label = "Install group for " .. decl.Name,
		Pass = my_pass,
		Dependencies = copies
	}
end

nodegen.add_evaluator("Install", _mt, blueprint)
