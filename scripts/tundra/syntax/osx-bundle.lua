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

-- osx-bundle.lua - Support for Max OS X bundles

module(..., package.seeall)

local nodegen = require "tundra.nodegen"
local files = require "tundra.syntax.files"
local path = require "tundra.path"
local util = require "tundra.util"

local function eval_osx_bundle(generator, env, decl, passes)
	local bundle_dir = assert(decl.Target)
	local contents = bundle_dir .. "/Contents"
	local pass = passes[decl.Pass]
	local deps = {}
	local copy_deps = {}

	for _, dep in util.nil_ipairs(decl.Depends) do
		deps[#deps+1] = generator:get_node_of(dep)
	end

	local infoplist = assert(decl.InfoPList)
	copy_deps[#copy_deps+1] = files.hardlink_file(env, decl.InfoPList, contents .. "/Info.plist", pass, deps)

	if decl.PkgInfo then
		copy_deps[#copy_deps+1] = files.hardlink_file(env, decl.PkgInfo, contents .. "/PkgInfo", pass, deps)
	end

	if decl.Executable then
		local basename = select(2, path.split(decl.Executable))
		copy_deps[#copy_deps+1] = files.hardlink_file(env, decl.Executable, contents .. "/MacOS/" .. basename, pass, deps)
	end

	local dirs = {
		{ Tag = "Resources", Dir = contents .. "/Resources/" },
		{ Tag = "MacOSFiles", Dir = contents .. "/MacOS/" },
	}

	for _, params in ipairs(dirs) do
		local function do_copy(fn)
			local basename = select(2, path.split(fn))
			copy_deps[#copy_deps+1] = files.hardlink_file(env, fn, params.Dir .. basename, pass, deps)
		end

		local items = decl[params.Tag]
		for _, fn in util.nil_ipairs(nodegen.flatten_list(env:get('BUILD_ID'), items)) do
			if type(fn) == "string" then
				do_copy(fn)
			else
				local node = fn(env)
				deps[#deps+1] = node
				local files = {}
				node:insert_output_files(files)
				for _, fn in ipairs(files) do
					do_copy(fn)
				end
			end
		end
	end

	return env:make_node {
		Pass = pass,
		Label = "OsxBundle " .. decl.Target,
		Dependencies = util.merge_arrays_2(deps, copy_deps),
	}
end

local function compile_nib(args, passes)
	local pass = passes[args.Pass]
	local src = assert(args.Source)
	local dst = assert(args.Target)

	return function (env)
		return env:make_node {
			Pass = pass,
			Label = "CompileNib $(@)",
			Action = "$(NIBCC)",
			InputFiles = { src },
			OutputFiles = { "$(OBJECTDIR)/" .. dst },
		}
	end
end

function apply(decl_parser, passes)
	nodegen.add_evaluator("OsxBundle", function(generator, env, decl)
		return eval_osx_bundle(generator, env, decl, passes)
	end)

	decl_parser:add_source_generator("CompileNib", function (args)
		return compile_nib(args, passes)
	end)
end
