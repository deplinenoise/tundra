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

-- embed_lua.lua - Embed lua files

module(..., package.seeall)

local util = require "tundra.util"
local path = require "tundra.path"
local glob = require "tundra.syntax.glob"
local nodegen = require "tundra.nodegen"

local lua_exts = { ".lua" }
local luac_mt_ = nodegen.create_eval_subclass {}

local function luac(env, src, pass)
	local target = "$(OBJECTDIR)/" .. path.drop_suffix(src) .. ".luac" 
	return target, env:make_node {
		Pass = pass,
		Label = "LuaC $(@)",
		Action = "$(LUAC) -o $(@) -- $(<)",
		InputFiles = { src },
		OutputFiles = { target },
		ImplicitInputs = { "$(LUAC)" },
	}
end

function luac_mt_:create_dag(env, data, deps)
	local files = {}
	local deps = {}
	local inputs = {}
	local action_fragments = {}

	for _, base_dir in ipairs(data.Dirs) do
		local lua_files = glob.Glob { Dir = base_dir, Extensions = lua_exts }
		local dir_len = base_dir:len()
		for _, filename in pairs(lua_files) do
			local rel_name = filename:sub(dir_len+2)
			local pkg_name = rel_name:gsub("[/\\]", "."):gsub("%.lua$", "")
			inputs[#inputs + 1] = filename
			if env:get("LUA_EMBED_ASCII", "no") == "no" then
				files[#files + 1], deps[#deps + 1] = luac(env, filename, data.Pass)
			else
				files[#files + 1] = filename
			end
			action_fragments[#action_fragments + 1] = pkg_name
			action_fragments[#action_fragments + 1] = files[#files]
		end
	end

	return env:make_node {
		Label = "EmbedLuaSources $(@)",
		Pass = data.Pass,
		Action = "$(GEN_LUA_DATA) " .. table.concat(action_fragments, " ") .. " > $(@)",
		InputFiles = inputs,
		OutputFiles = { "$(OBJECTDIR)/" .. data.OutputFile },
		Dependencies = deps,
		ImplicitInputs = { "$(GEN_LUA_DATA)" },
	}
end

local blueprint = {
	Dirs = { Type = "table", Required = "true" },
	OutputFile = { Type = "string", Required = "true" },
}

nodegen.add_evaluator("EmbedLuaSources", luac_mt_, blueprint)
