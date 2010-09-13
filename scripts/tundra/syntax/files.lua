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

-- files.lua - File management utilities, e.g. copying

module(..., package.seeall)

function copy_file(env, src, target, pass)
	return env:make_node {
		Pass = pass,
		Label = "CopyFile $(@)",
		Action = "$(_COPY_FILE)",
		InputFiles = { src },
		OutputFiles = { target },
	}
end

-- CopyFile syntax - Copy a source file to a destination file.
--
-- Synopsis:
-- CopyFile { Source = "...", Target = "..." }

function apply(decl_parser, passes)
	decl_parser:add_source_generator("CopyFile", function (args)
		return function (env)
			local src = args.Source
			local target = args.Target
			local pass = args.Pass and passes[args.Pass] or nil
			assert(src and type(src) == "string")
			assert(src and type(src) == "string")
			return copy_file(env, src, target, pass)
		end
	end)
end


