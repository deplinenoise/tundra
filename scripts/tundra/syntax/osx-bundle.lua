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

_osx_bundle_mt = nodegen.create_eval_subclass { }
_compile_nib_mt = nodegen.create_eval_subclass { }

function _osx_bundle_mt:create_dag(env, data, deps)
	local bundle_dir = data.Target
	local pass = data.Pass
	local contents = bundle_dir .. "/Contents"
	local copy_deps = {}

	local infoplist = data.InfoPList
	copy_deps[#copy_deps+1] = files.hardlink_file(env, data.InfoPList, contents .. "/Info.plist", pass, deps)

	if data.PkgInfo then
		copy_deps[#copy_deps+1] = files.hardlink_file(env, data.PkgInfo, contents .. "/PkgInfo", pass, deps)
	end

	if data.Executable then
		local basename = select(2, path.split(data.Executable))
		copy_deps[#copy_deps+1] = files.hardlink_file(env, data.Executable, contents .. "/MacOS/" .. basename, pass, deps)
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

		local items = data[params.Tag]
		for _, dep in util.nil_ipairs(nodegen.flatten_list(env:get('BUILD_ID'), items)) do
			if type(dep) == "string" then
				do_copy(dep)
			else
				local node = dep:get_dag(env)
				print(node)
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
		Label = "OsxBundle " .. data.Target,
		Dependencies = util.merge_arrays_2(deps, copy_deps),
	}
end

function _compile_nib_mt:create_dag(env, data, deps)
	return env:make_node {
		Pass = data.Pass,
		Label = "CompileNib $(@)",
		Action = "$(NIBCC)",
		Dependencies = deps,
		InputFiles = { data.Source },
		OutputFiles = { "$(OBJECTDIR)/" .. data.Target },
	}
end

nodegen.add_evaluator("OsxBundle", _osx_bundle_mt, {
	Target = { Type = "string", Required = true, Help = "Target .app directory name" },
	Executable = { Type = "string", Help = "Executable to embed" },
	InfoPList = { Type = "string", Required = true, Help = "Info.plist file" },
	PkgInfo = { Type = "string", Help = "PkgInfo file" },
	Resources = { Type = "filter_table", Help = "Files to copy to 'Resources'" },
	MacOSFiles = { Type = "filter_table", Help = "Files to copy to 'MacOS'" },
})

nodegen.add_evaluator("CompileNib", _compile_nib_mt, {
	Source = { Type = "string", Required = true },
	Target = { Type = "string", Required = true },
})

