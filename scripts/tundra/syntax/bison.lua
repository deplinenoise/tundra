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
			local targetbase = "$(OBJECTDIR)/bisongen_" .. path.get_filename_base(src)
			local out_ext = ".c"

			local langopt = ""
			if args.Language then
				out_ext = assert(args.Extension, "Must specify a file extension if language is specified")
				langopt = "--language=" .. args.Language
			end

			local out_src = targetbase .. out_ext
			local defopt = ""
			local outputs = { out_src }
			if args.TokenDefines then
				local out_hdr = targetbase .. ".h"
				defopt = "--defines=" .. out_hdr
				outputs[#outputs + 1] = out_hdr
			end
			return env:make_node {
				Pass = assert(passes[args.Pass], "Must specify a Pass for Bison"),
				Label = "Bison $(@)",
				Action = "$(BISON) $(BISONOPT)" .. " " .. langopt .. " " .. defopt .. " --output-file=$(@:[1]) $(<)",
				InputFiles = { src },
				OutputFiles = outputs,
			}
		end
	end)

	decl_parser:add_source_generator("Flex", function (args)
		return function (env)
			local input = assert(args.Source, "Must specify a Source for Flex")

			out_ext = ".c"
			if args.Extension then
				out_ext = args.Extension
			end

			local out_src = "$(OBJECTDIR)/flexgen_" .. path.get_filename_base(input) .. out_ext
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

