local _outer_env = ...
local depgraph = require("tundra.depgraph")
local util = require("tundra.util")
local path = require("tundra.path")
local native = require("tundra.native")

local function get_cpp_scanner(env, fn)
	local function new_scanner()
		local paths = util.map(env:get_list("CPPPATH"), function (v) return env:interpolate(v) end)
		return native_engine:make_cpp_scanner(paths)
	end
	return env:memoize("CPPPATH", "_cpp_scanner", new_scanner)
end

do
	local _anyc_compile = function(env, args, label, action)
		local function obj_fn(fn)
			if fn:match('^%$%(OBJECTDIR%)') then
				return path.drop_suffix(fn) .. '$(OBJECTSUFFIX)'
			else
				return '$(OBJECTDIR)/' .. path.drop_suffix(fn) .. '$(OBJECTSUFFIX)'
			end
		end
		local fn = args.Source
		assert(type(fn) == "string", "argument must be a string")
		local object_fn = obj_fn(fn)
		return env:make_node {
			Label = 'Cc $(@)',
			Pass = args.Pass,
			Action = "$(CCCOM)",
			InputFiles = { fn },
			OutputFiles = { object_fn },
			Scanner = get_cpp_scanner(env, fn),
			Dependencies = args.Dependencies,
		}
	end

	local cc_compile = function(env, args)
		return _anyc_compile(env, args, "Cc $(@)", "$(CCCOM)")
	end

	local cxx_compile = function(env, args)
		return _anyc_compile(env, args, "C++ $(@)", "$(CXXCOM)")
	end

	local h_compile = function(env, args)
		return nil, true -- supress this file
	end

	_outer_env.make.CcObject = cc_compile
	_outer_env:register_implicit_make_fn("h", h_compile)
	_outer_env:register_implicit_make_fn("c", cc_compile)
	_outer_env:register_implicit_make_fn("cpp", cxx_compile)
	_outer_env:register_implicit_make_fn("cc", cxx_compile)
	_outer_env:register_implicit_make_fn("cxx", cxx_compile)
end

_outer_env.make.Object = function(env, args)
	local input = args.Source

	-- Allow premade objects to be passed here to e.g. Library's Sources list
	if type(input) == "table" then
		return input
	end

	local implicitMake = env:get_implicit_make_fn(input)
	return implicitMake(env, args)
end


-- Analyze source list, returning list of input files and list of dependencies.
--
-- This is so you can pass a mix of actions producing files and regular
-- filenames as inputs to the next step in the chain and the output files of
-- such nodes will be used automatically.
--
-- list - list of source files and nodes that produce source files
-- suffixes - acceptable source suffixes to pick up from nodes in source list
-- transformer (optional) - transformer function to make nodes from plain filse
--
local function analyze_sources(list, suffixes, transformer)
	if type(list) ~= "table" or #list < 1 then
		error("no sources provided")
	end

	local inputs = {}
	local deps = {}

	for _, src in ipairs(list) do

		if type(src) == "string" then
			if transformer then
				local old = src
				local supress
				src, supress = transformer(src)
				if not supress and not src then
					error("transformer produced nil value for " .. old)
				end
			end
		end


		if native.is_node(src) then
			src:insert_output_files(inputs, suffixes)
			for _, x in ipairs(inputs) do
				assert(x)
			end
			deps[#deps + 1] = src
		else
			inputs[#inputs + 1] = src
		end
	end

	if #inputs == 0 then
		error("no suitable input files (" .. util.tostring(suffixes) .. ") found in list: " .. util.tostring(list))
	end

	return inputs, deps
end

local function link_common(env, args, label, action, suffix, suffixes)
	local function obj_hook(fn)
		return env.make.Object { Source = fn, Pass = args.Pass }
	end
	local exts = util.map(suffixes, function (x) return env:get(x) end)
	if #exts == 0 then
		error(label .. ": no extensions specified", 1)
	end
	local inputs, deps = analyze_sources(args.Sources, exts, obj_hook)
	local libnode = env:make_node {
		Label = label .. " $(@)",
		Pass = args.Pass,
		Action = action,
		InputFiles = inputs,
		OutputFiles = { util.get_named_arg(args, "Target") .. suffix },
		Dependencies = util.merge_arrays_2(deps, args.Dependencies),
	}
	return libnode
end

local common_suffixes = { "LIBSUFFIX", "OBJECTSUFFIX" }

_outer_env.make.Library = function (env, args)
	return link_common(env, args, "Library", "$(LIBCOM)", "$(LIBSUFFIX)", common_suffixes)
end

_outer_env.make.Program = function (env, args)
	return link_common(env, args, "Program", "$(PROGCOM)", "$(PROGSUFFIX)", common_suffixes)
end

local csSourceExts = { ".cs" }

_outer_env.make.CSharpExe = function (env, args)
	local inputs, deps = analyze_sources(args.Sources, csSourceExts)
	return env:make_node {
		Pass = args.Pass,
		Label = "C# Exe $(@)",
		Action = "$(CSCEXECOM)",
		InputFiles = inputs,
		OutputFiles = { env:interpolate(util.get_named_arg(args, "Target")) },
		Dependencies = util.merge_arrays_2(deps, args.Dependencies),
	}
end

