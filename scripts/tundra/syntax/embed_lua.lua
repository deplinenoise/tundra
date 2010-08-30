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

local lua_exts = { ".lua" }

function apply(decl_parser, passes)
	local function luac(env, src)
		local target = "$(OBJECTDIR)/" .. path.drop_suffix(src) .. ".luac" 
		return target, env:make_node {
			Pass = passes.LuaCompile,
			Label = "LuaC $(@)",
			Action = "$(LUAC) -o $(@) -- $(<)",
			InputFiles = { src },
			OutputFiles = { target },
			ImplicitInputs = { "$(LUAC)" },
		}
	end

	decl_parser:add_source_generator("EmbedLuaSources", function (args)
		return function (env)
			local files = {}
			local deps = {}
			local inputs = {}
			local action_fragments = {}

			for _, base_dir in ipairs(args.Dirs) do
				local lua_files = glob.Glob { Dir = base_dir, Extensions = lua_exts }
				local dir_len = base_dir:len()
				for _, filename in pairs(lua_files) do
					local rel_name = filename:sub(dir_len+2)
					local pkg_name = rel_name:gsub("[/\\]", "."):gsub("%.lua$", "")
					inputs[#inputs + 1] = filename
					files[#files + 1], deps[#deps + 1] = luac(env, filename)
					action_fragments[#action_fragments + 1] = pkg_name
					action_fragments[#action_fragments + 1] = files[#files]
				end
			end

			return env:make_node {
				Label = "EmbedLuaSources $(@)",
				Pass = passes.Tundra,
				Action = "$(GEN_LUA_DATA) " .. table.concat(action_fragments, " ") .. " > $(@)",
				InputFiles = inputs,
				OutputFiles = { "$(OBJECTDIR)/" .. args.OutputFile },
				Dependencies = deps,
				ImplicitInputs = { "$(GEN_LUA_DATA)" },
			}
		end
	end)
end
