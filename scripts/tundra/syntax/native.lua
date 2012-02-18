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

module(..., package.seeall)

local util = require "tundra.util"
local nodegen = require "tundra.nodegen"
local path = require "tundra.path"

local _native_mt = nodegen.create_eval_subclass {
	DeclToEnvMappings = {
		Libs = "LIBS",
		Defines = "CPPDEFS",
		Includes = "CPPPATH",
		Frameworks = "FRAMEWORKS",
		LibPaths = "LIBPATH",
	},
} 

local _object_mt = nodegen.create_eval_subclass({
	Suffix = "$(OBJSUFFIX)",
	Prefix = "",
	Action = "$(OBJCOM)",
	Label = "Object $(@)",
}, _native_mt)

local _program_mt = nodegen.create_eval_subclass({
	Suffix = "$(PROGSUFFIX)",
	Prefix = "$(PROGPREFIX)",
	Action = "$(PROGCOM)",
	Label = "Program $(@)",
}, _native_mt)

local _staticlib_mt = nodegen.create_eval_subclass({
	Suffix = "$(LIBSUFFIX)",
	Prefix = "$(LIBPREFIX)",
	Action = "$(LIBCOM)",
	Label = "StaticLibrary $(@)",
}, _native_mt)

local _shlib_mt = nodegen.create_eval_subclass({
	Suffix = "$(SHLIBSUFFIX)",
	Prefix = "$(SHLIBPREFIX)",
	Action = "$(SHLIBCOM)",
	Label = "SharedLibrary $(@)",
}, _native_mt)

local _extlib_mt = nodegen.create_eval_subclass({
	Suffix = "",
	Prefix = "",
	Label = "",
}, _native_mt)

local _is_native_mt = util.make_lookup_table { _object_mt, _program_mt, _staticlib_mt, _shlib_mt, _extlib_mt }

function _native_mt:customize_env(env, raw_data)
	if env:get('GENERATE_PDB', '0') ~= '0' then
		env:set('_PDB_FILE', "$(OBJECTDIR)/" .. raw_data.Name .. ".pdb")
		env:set('_USE_PDB_CC', '$(_USE_PDB_CC_OPT)')
		env:set('_USE_PDB_LINK', '$(_USE_PDB_LINK_OPT)')
	end

	local pch = raw_data.PrecompiledHeader

	if pch then
		assert(pch.Header)
		env:set('_PCH_FILE', "$(OBJECTDIR)/" .. raw_data.Name .. ".pch")
		env:set('_USE_PCH', '$(_USE_PCH_OPT)')
		env:set('_PCH_HEADER', pch.Header)
	end
end

function _native_mt:create_dag(env, data, input_deps)
	local build_id = env:get("BUILD_ID")
	local my_pass = data.Pass
	local pch_output
	local gen_pch_node
	local sources = data.Sources
	local libsuffix = { env:get("LIBSUFFIX") }

	-- Link with libraries in dependencies.
	for _, dep in util.nil_ipairs(data.Depends) do
		if dep.Keyword == "SharedLibrary" then

			-- On Win32 toolsets, we need foo.lib
			-- On UNIX toolsets, we need -lfoo
			local target = dep.Decl.Target or dep.Decl.Name
			target = target .. "$(SHLIBLINKSUFFIX)"
			env:append('LIBS', target)

		elseif dep.Keyword == "StaticLibrary" then
			local node = dep:get_dag(env:get_parent())
			node:insert_output_files(sources, libsuffix)
		else

			--[[

			A note about win32 import libraries:

			It is tempting to add an implicit input dependency on the import
			library of the linked-to shared library here; but this would be
			suboptimal:

			1. Because there is a dependency between the nodes themselves,
			the import library generation will always run before this link
			step is run. Therefore, the import lib will always exist and be
			updated before this link step runs.

			2. Because the import library is regenerated whenever the DLL is
			relinked we would have to custom-sign it (using a hash of the
			DLLs export list) to avoid relinking the executable all the
			time when only the DLL's internals change.

			3. The DLL's export list should be available in headers anyway,
			which is already covered in the compilation of the object files
			that actually uses those APIs.

			Therefore the best way right now is to not tell Tundra about the
			import lib at all and rely on header scanning to pick up API
			changes.

			An implicit input dependency would be needed however if someone
			is doing funky things with their import library (adding
			non-linker-generated code for example). These cases are so rare
			that we can safely put them off.

			]]--
		end
	end

	local pch = data.PrecompiledHeader
	if pch then
		local pch_pass = nil
		if pch.Pass then
			pch_pass = nodegen.resolve_pass(pch.Pass)
		end
		if not pch_pass then
			croak("%s: PrecompiledHeader requires a valid Pass", data.Name)
		end
		gen_pch_node = env:make_node {
			Label = "Precompiled header $(@)",
			Pass = pch_pass,
			Action = "$(PCHCOMPILE)",
			InputFiles = { pch.Source, pch.Header },
			OutputFiles = { "$(_PCH_FILE)" },
		}
	end

	local aux_outputs = env:get_list("AUX_FILES_" .. self.Label:upper(), {})

	if env:get('GENERATE_PDB', '0') ~= '0' then
		aux_outputs[#aux_outputs + 1] = "$(_PDB_FILE)"
	end

	local targets = nil

	if self.Action then
		targets = { nodegen.get_target(data, self.Suffix, self.Prefix) }
	end

	local deps = {}

	if gen_pch_node then
		deps = util.merge_arrays_2(deps, { gen_pch_node })
	end

	deps = util.merge_arrays_2(deps, input_deps)
	deps = util.uniq(deps)

	return env:make_node {
		Label = self.Label,
		Pass = data.Pass,
		Action = self.Action,
		InputFiles = data.Sources,
		OutputFiles = targets,
		AuxOutputFiles = aux_outputs,
		Dependencies = deps,
		-- Play it safe and delete the output files of this node before re-running it.
		-- Solves iterative issues with e.g. AR
		OverwriteOutputs = false,
	}
end

local native_blueprint = {
	Name = {
		Required = true,
		Help = "Set output (base) filename",
		Type = "string",
	},
	Sources = {
		Required = true,
		Help = "List of source files",
		Type = "source_list",
		ExtensionKey = "NATIVE_SUFFIXES",
	},
	Target = {
		Help = "Override target location",
		Type = "string",
	},
	PrecompiledHeader = {
		Help = "Enable precompiled header (if supported)",
		Type = "table",
	},
}

local external_blueprint = {
	Name = {
		Required = true,
		Help = "Set name of the external library",
		Type = "string",
	},
}

function _extlib_mt:create_dag(env, data, input_deps)
	return env:make_node {
		Label = "Dummy node for " .. data.Name,
		Pass = data.Pass,
		Dependencies = input_deps,
	}
end

nodegen.add_evaluator("Object", _object_mt, native_blueprint)
nodegen.add_evaluator("Program", _program_mt, native_blueprint)
nodegen.add_evaluator("StaticLibrary", _staticlib_mt, native_blueprint)
nodegen.add_evaluator("SharedLibrary", _shlib_mt, native_blueprint)
nodegen.add_evaluator("ExternalLibrary", _extlib_mt, external_blueprint)
