local depgraph = require("tundra.depgraph")
local util = require("tundra.util")
local path = require("tundra.path")
local native = require("tundra.native")

local function get_cpp_scanner(env, fn)
	local function new_scanner()
		local paths = util.map(env:GetList("CPPPATH"), function (v) return env:Interpolate(v) end)
		return native_engine:make_cpp_scanner(paths)
	end
	return env:memoize("CPPPATH", "_cpp_scanner", new_scanner)
end

do
	local cc_compile = function(env, args)
		local function GetObjectFilename(fn)
			return '$(OBJECTDIR)/' .. path.DropSuffix(fn) .. '$(OBJECTSUFFIX)'
		end
		local fn = args.Source
		assert(type(fn) == "string", "argument must be a string")
		local object_fn = GetObjectFilename(fn)
		local node = env:MakeNode {
			Label = 'Cc $(@)',
			Pass = args.Pass,
			Action = "$(CCCOM)",
			InputFiles = { fn },
			OutputFiles = { object_fn },
			Scanner = get_cpp_scanner(env, fn),
			Dependencies = args.Dependencies,
		}
		return node
	end

	DefaultEnvironment.Make.CcObject = cc_compile
	DefaultEnvironment:RegisterImplicitMakeFn("c", cc_compile)
end

DefaultEnvironment.Make.Object = function(env, args)
	local input = args.Source

	-- Allow premade objects to be passed here to e.g. Library's Sources list
	if type(input) == "table" then
		return input
	end

	local implicitMake = env:GetImplicitMakeFn(input)
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
local function AnalyzeSources(list, suffixes, transformer)
	if type(list) ~= "table" or #list < 1 then
		error("no sources provided")
	end

	local inputs = {}
	local deps = {}

	for _, src in ipairs(list) do
		if type(src) == "string" then
			if transformer then
				src = transformer(src)
			end
		end

		if native.is_node(src) then
			src:insert_output_files(inputs, suffixes)
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

local function LinkCommon(env, args, label, action, suffix, suffixes)
	local function obj_hook(fn)
		return env.Make.Object { Source = fn, Pass = args.Pass }
	end
	local exts = util.map(suffixes, function (x) return env:Get(x) end)
	if #exts == 0 then
		error(label .. ": no extensions specified", 1)
	end
	local inputs, deps = AnalyzeSources(args.Sources, exts, obj_hook)
	local libnode = env:MakeNode {
		Label = label .. " $(@)",
		Pass = args.Pass,
		Action = action,
		InputFiles = inputs,
		OutputFiles = { util.GetNamedArg(args, "Target") .. suffix },
		Dependencies = util.MergeArrays2(deps, args.Dependencies),
	}
	return libnode
end

local common_suffixes = { "LIBSUFFIX", "OBJECTSUFFIX" }

DefaultEnvironment.Make.Library = function (env, args)
	return LinkCommon(env, args, "Library", "$(LIBCOM)", "$(LIBSUFFIX)", common_suffixes)
end

DefaultEnvironment.Make.Program = function (env, args)
	return LinkCommon(env, args, "Program", "$(PROGCOM)", "$(PROGSUFFIX)", common_suffixes)
end

local csSourceExts = { ".cs" }

DefaultEnvironment.Make.CSharpExe = function (env, args)
	local inputs, deps = AnalyzeSources(args.Sources, csSourceExts)
	return env:MakeNode {
		Pass = args.Pass,
		Label = "C# Exe $(@)",
		Action = "$(CSCEXECOM)",
		InputFiles = inputs,
		OutputFiles = { util.GetNamedArg(args, "Target") },
		Dependencies = util.MergeArrays2(deps, args.Dependencies),
	}
end

