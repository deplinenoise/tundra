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

local function eval_install(generator, env, decl, passes)
	local build_id = env:get("BUILD_ID")
	local my_pass = generator:resolve_pass(decl.Pass)
	local sources = nodegen.flatten_list(build_id, decl.Sources)
	local target_dir = decl.TargetDir

	assert(type(target_dir) == "string")

	local copies = {}

	for _, src in util.nil_ipairs(sources) do
		local base_fn = select(2, path.split(src))
		local target = target_dir .. '/' .. base_fn
		copies[#copies + 1] = files.copy_file(env, src, target, my_pass)
	end

	return env:make_node {
		Label = "Install group for " .. decl.Name,
		Pass = my_pass,
		Dependencies = copies
	}
end

function apply(decl_parser, passes)
	nodegen.add_evaluator("Install", function(generator, env, decl)
		return eval_install(generator, env, decl, passes)
	end)
end
