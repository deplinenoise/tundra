-- Copyright 2011 Andreas Fredriksson
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

-- generic-asm.lua - Generic assembly language support

module(..., package.seeall)

local path = require "tundra.path"

-- Register implicit make functions for assembly files.
-- These functions are called to transform source files in unit lists into
-- object files. This function is registered as a setup function so it will be
-- run after user modifications to the environment, but before nodes are
-- processed. This way users can override the extension lists.
local function generic_asm_setup(env)
	local _assemble = function(env, pass, fn)
		local object_fn

		-- Drop leading $(OBJECTDIR)[/\\] in the input filename.
		do
			local pname = fn:match("^%$%(OBJECTDIR%)[/\\](.*)$")
			if pname then
				object_fn = pname
			else
				object_fn = fn
			end
		end

		-- Compute path under OBJECTDIR we want for the resulting object file.
		-- Replace ".." with "dotdot" to avoid creating files outside the
		-- object directory.
		do
			local relative_name = path.drop_suffix(object_fn:gsub("%.%.", "dotdot"))
			object_fn = "$(OBJECTDIR)/$(UNIT_PREFIX)/" .. relative_name .. '$(OBJECTSUFFIX)'
		end

		return env:make_node {
			Label = 'Asm $(@)',
			Pass = pass,
			Action = "$(ASMCOM)",
			InputFiles = { fn },
			OutputFiles = { object_fn },
		}
	end

	for _, ext in ipairs(env:get_list("ASM_EXTS")) do
		env:register_implicit_make_fn(ext, _assemble)
	end
end

function apply(_outer_env, options)

	_outer_env:add_setup_function(generic_asm_setup)

	_outer_env:set_many {
		["ASM_EXTS"] = { ".s", ".asm" },
		["CPPPATH"] = "",
		["ASMDEFS"] = "",
		["ASMDEFS_DEBUG"] = "",
		["ASMDEFS_PRODUCTION"] = "",
		["ASMDEFS_RELEASE"] = "",
		["ASMOPTS"] = "",
		["ASMOPTS_DEBUG"] = "",
		["ASMOPTS_PRODUCTION"] = "",
		["ASMOPTS_RELEASE"] = "",
	}
end

