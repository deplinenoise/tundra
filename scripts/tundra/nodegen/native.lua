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

local util = require("tundra.util")
local nodegen = require("tundra.nodegen")
local path = require("tundra.path")

local decl_to_env_mappings = {
	Libs = "LIBS",
	Defines = "CPPDEFS",
	Includes = "CPPPATH",
	Frameworks = "FRAMEWORKS",
	LibPaths = "LIBPATH",
}

local function eval_native_unit(generator, env, label, suffix, command, decl)
	local build_id = env:get("BUILD_ID")
	local pch = decl.PrecompiledHeader
	local my_pass = generator:resolve_pass(decl.Pass)
	local pch_output
	local gen_pch_node

	for decl_key, env_key in pairs(decl_to_env_mappings) do
		local data = decl[decl_key]
		for _, item in util.nil_ipairs(nodegen.flatten_list(build_id, data)) do
			env:append(env_key, item)
		end
	end

	if pch then
		pch_output = "$(OBJECTDIR)/" .. decl.Name .. ".pch"
		env:set('_PCH_FILE', pch_output)
		env:set('_USE_PCH', '$(_USE_PCH_OPT)')
		env:set('_PCH_HEADER', pch.Header)
		gen_pch_node = env:make_node {
			Label = "Precompiled header $(@)",
			Pass = my_pass,
			Action = "$(PCHCOMPILE)",
			InputFiles = { pch.Source, pch.Header },
			OutputFiles = { pch_output },
		}
	end

	local function implicit_make(source_file)
		local t = type(source_file)
		if t == "table" then
			return source_file
		end
		assert(t == "string")

		local my_env = env
		
		if pch then
			for _, except in util.nil_ipairs(pch.Exclude) do
				if source_file == except then
					my_env = env:clone()
					my_env:set('_PCH_FILE', '')
					my_env:set('_USE_PCH', '')
					my_env:set('_PCH_HEADER', '')
					break
				end
			end

		end

		local make = my_env:get_implicit_make_fn(source_file)
		if make then
			return make(my_env, my_pass, source_file)
		else
			return nil
		end
	end

	local exts = env:get_list("NATIVE_SUFFIXES")
	local deps = generator:resolve_deps(build_id, decl.Depends)
	local source_files = nodegen.flatten_list(build_id, decl.Sources)
	local sources = generator:resolve_sources(env, { source_files, deps }, {}, decl.SourceDir)
	local inputs, ideps = generator:analyze_sources(sources, exts, implicit_make)
	local targets = { generator:get_target(decl, suffix) }

	if gen_pch_node then
		deps = util.merge_arrays_2(deps, { gen_pch_node })
	end

	deps = util.merge_arrays_2(deps, ideps)
	deps = util.uniq(deps)
	local libnode = env:make_node {
		Label = label .. " $(@)",
		Pass = my_pass,
		Action = command,
		InputFiles = inputs,
		OutputFiles = targets,
		AuxOutputFiles = env:get_list("AUX_FILES_" .. label:upper(), {}),
		Dependencies = deps,
	}
	return libnode
end

nodegen.add_evaluator("Program", function (generator, env, decl)
	return eval_native_unit(generator, env, "Program", "$(PROGSUFFIX)", "$(PROGCOM)", decl)
end)

nodegen.add_evaluator("StaticLibrary", function (generator, env, decl)
	return eval_native_unit(generator, env, "StaticLib", "$(LIBSUFFIX)", "$(LIBCOM)", decl)
end)

nodegen.add_evaluator("SharedLibrary", function (generator, env, decl)
	return eval_native_unit(generator, env, "SharedLib", "$(SHLIBSUFFIX)", "$(SHLIBCOM)", decl)
end)
