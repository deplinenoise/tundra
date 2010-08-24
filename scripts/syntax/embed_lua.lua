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

local decl_parser, passes = ...
local util = require "tundra.util"
local path = require "tundra.path"

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
	local sources = util.flatten(args.Sources)
	return function (env)
		local files = {}
		local deps = {}
		for _, src in ipairs(sources) do
			files[#files + 1], deps[#deps + 1] = luac(env, src)
		end
		return env:make_node {
			Label = "EmbedLuaSources $(@)",
			Pass = passes.Tundra,
			Action = "$(GEN_LUA_DATA) $(<) > $(@)",
			InputFiles = files,
			OutputFiles = { "$(OBJECTDIR)/" .. args.OutputFile },
			Dependencies = deps,
			ImplicitInputs = { "$(GEN_LUA_DATA)" },
		}
	end
end)

