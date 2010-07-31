local _generator = ...
local util = require("tundra.util")
local nodegen = require("tundra.nodegen")
local path = require("tundra.path")

local function install_libs(unit_env, decl)
	for _, item in util.nil_ipairs(nodegen.flatten_list(unit_env:get('BUILD_ID'), decl.Libs)) do
		unit_env:append("LIBS", item)
	end
end

local function install_defines(unit_env, decl)
	for _, item in util.nil_ipairs(nodegen.flatten_list(unit_env:get('BUILD_ID'), decl.Defines)) do
		unit_env:append("CPPDEFS", item)
	end
end

local function install_includes(unit_env, decl)
	for _, item in util.nil_ipairs(nodegen.flatten_list(unit_env:get('BUILD_ID'), decl.Includes)) do
		unit_env:append("CPPPATH", item)
	end
end

function _generator:eval_native_unit(env, label, suffix, command, decl)
	local build_id = env:get("BUILD_ID")
	local pch = decl.PrecompiledHeader
	local pch_output
	local gen_pch_node

	install_libs(env, decl)
	install_defines(env, decl)
	install_includes(env, decl)

	if pch then
		pch_output = "$(OBJECTDIR)/" .. decl.Name .. ".pch"
		env:set('_PCH_FILE', pch_output)
		env:set('_USE_PCH', '$(_USE_PCH_OPT)')
		env:set('_PCH_HEADER', pch.Header)
		gen_pch_node = env:make_node {
			Label = "Precompiled header $(@)",
			Pass = self:resolve_pass(decl.Pass),
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
			return make(my_env, self:resolve_pass(decl.Pass), source_file)
		else
			return nil
		end
	end

	local exts = env:get_list("NATIVE_SUFFIXES")
	local deps = self:resolve_deps(build_id, decl.Depends)
	local source_files = nodegen.flatten_list(build_id, decl.Sources)
	local sources = self:resolve_sources(env, { source_files, deps }, {}, decl.SourceDir)
	local inputs, ideps = self:analyze_sources(sources, exts, implicit_make)

	if gen_pch_node then
		deps = util.merge_arrays_2(deps, { gen_pch_node })
	end
	deps = util.merge_arrays_2(deps, ideps)
	deps = util.merge_arrays_2(deps, decl.Dependencies)
	deps = util.uniq(deps)
	local libnode = env:make_node {
		Label = label .. " $(@)",
		Pass = self:resolve_pass(decl.Pass),
		Action = command,
		InputFiles = inputs,
		OutputFiles = { self:get_target(decl, suffix) },
		Dependencies = deps,
	}
	return libnode
end

nodegen.add_evaluator("Program", function (self, env, decl)
	return self:eval_native_unit(env, "Program", "$(PROGSUFFIX)", "$(PROGCOM)", decl)
end)

nodegen.add_evaluator("StaticLibrary", function (self, env, decl)
	return self:eval_native_unit(env, "StaticLib", "$(LIBSUFFIX)", "$(LIBCOM)", decl)
end)

nodegen.add_evaluator("SharedLibrary", function (self, env, decl)
	return self:eval_native_unit(env, "SharedLib", "$(SHLIBSUFFIX)", "$(SHLIBCOM)", decl)
end)
