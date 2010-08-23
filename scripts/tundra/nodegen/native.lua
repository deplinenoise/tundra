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
	local dep_names = nodegen.flatten_list(build_id, decl.Depends)
	local deps = util.mapnil(dep_names, function(x) return generator:get_node_of(x) end)
	local pch_output
	local gen_pch_node

	-- Push Libs, Defines and so in into the environment of this unit.
	do
		local propagate_blocks = nil

		for _, dep_name in util.nil_ipairs(dep_names) do
			local dep_decl = generator.units[dep_name]
			local data = dep_decl.Decl.Propagate
			if data then
				propagate_blocks = propagate_blocks or {}
				propagate_blocks[#propagate_blocks + 1] = data
			end
		end

		for decl_key, env_key in pairs(decl_to_env_mappings) do
			local function push_bindings(data)
				for _, item in util.nil_ipairs(nodegen.flatten_list(build_id, data)) do
					env:append(env_key, item)
				end
			end

			-- First pick settings from our own unit.
			push_bindings(decl[decl_key])

			for _, data in util.nil_ipairs(propagate_blocks) do
				push_bindings(data[decl_key])
			end
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

	local aux_outputs = env:get_list("AUX_FILES_" .. label:upper(), {})

	if generator.variant.Options.GeneratePdb then
		local pdb_output = "$(OBJECTDIR)/" .. decl.Name .. ".pdb"
		env:set('_PDB_FILE', pdb_output)
		env:set('_USE_PDB_CC', '$(_USE_PDB_CC_OPT)')
		env:set('_USE_PDB_LINK', '$(_USE_PDB_LINK_OPT)')
		aux_outputs[#aux_outputs + 1] = pdb_output
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
	local source_files = nodegen.flatten_list(build_id, decl.Sources)
	local sources = generator:resolve_sources(env, { source_files, deps }, {}, decl.SourceDir)
	local inputs, ideps = generator:analyze_sources(sources, exts, implicit_make)

	if not command and #inputs > 0 then
		errorf("unit %s has sources even though it is marked external", decl.Name)
	end

	local targets = nil
	if suffix then
		targets = { generator:get_target(decl, suffix) }
	end

	if gen_pch_node then
		deps = util.merge_arrays_2(deps, { gen_pch_node })
	end

	deps = util.merge_arrays_2(deps, ideps)
	deps = util.uniq(deps)
	local libnode = env:make_node {
		Label = label,
		Pass = my_pass,
		Action = command,
		InputFiles = inputs,
		OutputFiles = targets,
		AuxOutputFiles = aux_outputs,
		Dependencies = deps,
		-- Play it safe and delete the output files of this node before re-running it.
		-- Solves iterative issues with e.g. AR
		OverwriteOutputs = false,
	}
	return libnode
end

nodegen.add_evaluator("Program", function (generator, env, decl)
	return eval_native_unit(generator, env, "Program $(@)", "$(PROGSUFFIX)", "$(PROGCOM)", decl)
end)

nodegen.add_evaluator("StaticLibrary", function (generator, env, decl)
	return eval_native_unit(generator, env, "StaticLib $(@)", "$(LIBSUFFIX)", "$(LIBCOM)", decl)
end)

nodegen.add_evaluator("SharedLibrary", function (generator, env, decl)
	return eval_native_unit(generator, env, "SharedLib $(@)", "$(SHLIBSUFFIX)", "$(SHLIBCOM)", decl)
end)

nodegen.add_evaluator("ExternalLibrary", function (generator, env, decl)
	return eval_native_unit(generator, env, "ExternalLibrary " .. decl.Name, nil, nil, decl)
end)
