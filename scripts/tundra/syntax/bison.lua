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

-- bison.lua - Support for GNU Bison
--
-- Users should set BISON and BISONOPT in the environment, and not fiddle with
-- any options that generate more output files.

module(..., package.seeall)

local nodegen = require "tundra.nodegen"
local path = require "tundra.path"

local _bison_mt = nodegen.create_eval_subclass {}
local _flex_mt = nodegen.create_eval_subclass {}

local bison_blueprint = {
	Source = { Required = true, Type = "string" },
	OutputFile = { Required = false, Type = "string" },
	TokenDefines = { Required = false, Type = "boolean" },
}

function _bison_mt:create_dag(env, data, deps)
	local src = data.Source
	local out_src
	if data.OutputFile then
		out_src = "$(OBJECTDIR)$(SEP)" .. data.OutputFile
	else
		local targetbase = "$(OBJECTDIR)$(SEP)bisongen_" .. path.get_filename_base(src)
		out_src = targetbase .. ".c"
	end
	local defopt = ""
	local outputs = { out_src }
	if data.TokenDefines then
		local out_hdr = path.drop_suffix(out_src) .. ".h"
		defopt = "--defines=" .. out_hdr
		outputs[#outputs + 1] = out_hdr
	end
	return env:make_node {
		Pass = data.Pass,
		Label = "Bison $(@)",
		Action = "$(BISON) $(BISONOPT) " .. defopt .. " --output-file=$(@:[1]) $(<)",
		InputFiles = { src },
		OutputFiles = outputs,
		Dependencies = deps,
	}
end

local flex_blueprint = {
	Source = { Required = true, Type = "string" },
	OutputFile = { Required = false, Type = "string" },
}

function _flex_mt:create_dag(env, data, deps)
	local input = data.Source
	local out_src
	if data.OutputFile then
		out_src = "$(OBJECTDIR)$(SEP)" .. data.OutputFile
	else
		local targetbase = "$(OBJECTDIR)$(SEP)flexgen_" .. path.get_filename_base(input)
		out_src = targetbase .. ".c"
	end
	return env:make_node {
		Pass = data.Pass,
		Label = "Flex $(@)",
		Action = "$(FLEX) $(FLEXOPT) --outfile=$(@) $(<)",
		InputFiles = { input },
		OutputFiles = { out_src },
	}
end

nodegen.add_evaluator("Flex", _flex_mt, flex_blueprint)
nodegen.add_evaluator("Bison", _bison_mt, bison_blueprint)
