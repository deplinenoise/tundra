local _generator = ...
local util = require("tundra.util")
local nodegen = require("tundra.nodegen")

local csSourceExts = { ".cs" }
local csResXExts = { ".resx" }

local function setup_refs_from_dependencies(env, deps)
	local dll_exts = { env:interpolate("$(CSLIBSUFFIX)") }
	local refs = {}
	for _, x in util.nil_ipairs(deps) do
		local outputs = {}
		x:insert_output_files(refs, dll_exts)
	end
	for _, r in ipairs(refs) do
		env:append("CSLIBS", r)
	end
end

local function setup_direct_refs(env, refs)
	for _, r in util.nil_ipairs(refs) do
		env:append("CSLIBS", r)
	end
end

local function setup_resources(generator, env, assembly_name, resx_files, pass)
	local result_files = {}
	local deps = {}
	local i = 1
	for _, resx in util.nil_ipairs(resx_files) do
		local basename = path.get_filename_base(resx)
		local result_file = string.format("$(OBJECTDIR)/_rescompile/%s.%s.resources", assembly_name, basename)
		result_files[i] = result_file
		deps[i] = env:make_node {
			Pass = pass,
			Label = "resgen $(@)",
			Action = "$(CSRESGEN)",
			InputFiles = { resx },
			OutputFiles = { result_file },
		}
		env:append("CSRESOURCES", result_file)
		i = i + 1
	end
	return result_files, deps
end

function _generator:eval_csharp_unit(env, label, suffix, command, decl)
	local deps = self:resolve_deps(env, decl.Depends)
	local sources = self:resolve_sources(env, { nodegen.flatten_list(env, decl.Sources), deps }, {}, decl.SourceDir)
	local resources = self:resolve_sources(env, nodegen.flatten_list(env, decl.Resources), {}, decl.SourceDir)
	local inputs, inputDeps = self:analyze_sources(sources, csSourceExts)
	local resourceInputs, resourceDeps = self:analyze_sources(resources, csResXExts)
	local pass = self:resolve_pass(decl.Pass)
	deps = util.merge_arrays_2(deps, inputDeps)
	deps = util.merge_arrays_2(deps, resourceDeps)
	local rfiles, rdeps = setup_resources(env, decl.Name, resourceInputs)
	deps = util.merge_arrays_2(deps, rdeps)

	setup_refs_from_dependencies(env, deps)
	setup_direct_refs(env, nodegen.flatten_list(env, decl.References))

	for _, path in util.nil_ipairs(nodegen.flatten_list(env, decl.RefPaths)) do
		env:append("CSLIBPATH", path)
	end

	return env:make_node {
		Pass = pass,
		Label = label .. " $(@)",
		Action = command,
		InputFiles = inputs,
		ImplicitInputs = rfiles,
		OutputFiles = { self:get_target(decl, suffix) },
		Dependencies = util.uniq(deps),
	}
end

nodegen.add_evaluator("CSharpExe", function (self, env, decl)
	return self:eval_csharp_unit(env, "CSharpExe", "$(CSPROGSUFFIX)", "$(CSCEXECOM)", decl)
end)

nodegen.add_evaluator("CSharpLib", function (self, env, decl)
	return self:eval_csharp_unit(env, "CSharpLib", "$(CSLIBSUFFIX)", "$(CSCLIBCOM)", decl)
end)
