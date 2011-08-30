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

local util = require "tundra.util"
local path = require "tundra.path"

function apply(decl_parser, passes)
	decl_parser:add_source_generator("Bison", function (args)
		return function (env)
			local src = assert(args.Source, "Must specify a Source for Bison")
			local out_src
			if args.OutputFile then
				out_src = "$(OBJECTDIR)$(SEP)" .. args.OutputFile
			else
				local targetbase = "$(OBJECTDIR)$(SEP)bisongen_" .. path.get_filename_base(src)
				out_src = targetbase .. ".c"
			end
			local defopt = ""
			local outputs = { out_src }
			if args.TokenDefines then
				local out_hdr = path.drop_suffix(out_src) .. ".h"
				defopt = "--defines=" .. out_hdr
				outputs[#outputs + 1] = out_hdr
			end
			return env:make_node {
				Pass = assert(passes[args.Pass], "Must specify a Pass for Bison"),
				Label = "Bison $(@)",
				Action = "$(BISON) $(BISONOPT) " .. defopt .. " --output-file=$(@:[1]) $(<)",
				InputFiles = { src },
				OutputFiles = outputs,
			}
		end
	end)

	decl_parser:add_source_generator("Flex", function (args)
		return function (env)
			local input = assert(args.Source, "Must specify a Source for Flex")
			local out_src
			if args.OutputFile then
				out_src = "$(OBJECTDIR)$(SEP)" .. args.OutputFile
			else
				local targetbase = "$(OBJECTDIR)$(SEP)flexgen_" .. path.get_filename_base(input)
				out_src = targetbase .. ".c"
			end
			return env:make_node {
				Pass = assert(passes[args.Pass], "Must specify a Pass for Flex"),
				Label = "Flex $(@)",
				Action = "$(FLEX) $(FLEXOPT) --outfile=$(@) $(<)",
				InputFiles = { input },
				OutputFiles = { out_src },
			}
		end
	end)
end

