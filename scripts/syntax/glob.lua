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

-- glob.lua - Glob syntax elements for declarative tundra.lua usage

local decl_parser = ...
local native = require "tundra.native"
local util = require "tundra.util"
local path = require "tundra.path"

local function glob(directory, recursive, filter_fn)
	local result = {}
	for dir, dirs, files in native.walk_path(directory) do
		for _, fn in ipairs(files) do
			local path = dir .. '/' .. fn
			if filter_fn(path) then
				result[#result + 1] = path
			end
		end
		if not recursive then
			util.clear_table(dirs)
		end
	end
	return result
end


local function fancy_glob(filters)
	local filters = {
		["win32"] = { Config = "win32-*-*" },
		["debug"] = { Config = "fisk-*-*" },
	}
	local results = {}

	-- add (empty) filtered lists to results
	for _, x in pairs(filters) do
		results[#results + 1] = x
	end

	for _, f in ipairs(files) do
		local filtered = false
		for pattern, list in pairs(filters) do
			if f:match(pattern) then
				filtered = true
				list[#list + 1] = f
			end
		end

		-- not matched by any filter
		if not filtered then
			results[#results + 1] = f
		end
	end

	return results
end

-- Glob syntax - Search for source files matching extension list
--
-- Synopsis:
--   Glob {
--      Dir = "...",
--      Extensions = { ".ext", ... },
--      [Recursive = false,]
--   }
--
-- Options:
--    Dir = "directory" (required)
--    - Base directory to search in
--
--	  Extensions = { ".ext1", ".ext2" } (required)
--	  - List of file extensions to include
--
--	  Recursive = boolean (optional, default: true)
--	  - Specified whether to recurse into subdirectories
decl_parser:add_source_generator("Glob", function (args)
	local recursive = args.Recursive
	if type(recursive) == "nil" then
		recursive = true
	end
	local extensions = assert(args.Extensions)
	local ext_lookup = util.make_lookup_table(extensions)
	return glob(args.Dir, recursive, function (fn)
		local ext = path.get_extension(fn)
		return ext_lookup[ext]
	end)
end)

-- FGlob syntax - Search for source files matching extension list with
-- configuration filtering
--
-- Usage:
--   FGlob {
--       Dir = "...",
--       Extensions = { ".ext", .... },
--       Filters = {
--         { Pattern = "/[Ww]in32/", Config = "win32-*-*" },
--         { Pattern = "/[Dd]ebug/", Config = "*-*-debug" },
--         ...
--       },
--       [Recursive = false],
--   }
decl_parser:add_source_generator("FGlob", function (args)
	-- Use the regular glob to fetch the file list.
	local files = Glob(args)
	local pats = {}
	local result = {}

	-- Construct a mapping from { Pattern = ..., Config = ... }
	-- to { Pattern = { Config = ... } } with new arrays per config that can be
	-- embedded in the source result.
	for _, fitem in ipairs(args.Filters) do
		local tab = { Config = assert(fitem.Config) }
		pats[assert(fitem.Pattern)] = tab
		result[#result + 1] = tab
	end

	-- Traverse all files and see if they match any configuration filters. If
	-- they do, stick them in matching list. Otherwise, just keep them in the
	-- main list. This has the effect of returning an array such as this:
	-- {
	--   { "foo.c"; Config = "abc-*-*" },
	--   { "bar.c"; Config = "*-*-def" },
	--   "baz.c", "qux.m"
	-- }
	for _, f in ipairs(files) do
		local filtered = false
		for filter, list in pairs(pats) do
			if f:match(filter) then
				filtered = true
				list[#list + 1] = f
				break
			end
		end
		if not filtered then
			result[#result + 1] = f
		end
	end
	return result
end)
