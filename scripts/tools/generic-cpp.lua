local depgraph = require("tundra.depgraph")
local util = require("tundra.util")
local path = require("tundra.path")

local function MakeCppScanner(env, fn)
	return {
		Type = "cpp",
		IncludePaths = util.map(env:GetList("CPPPATH"), function (v) env:Interpolate(v) end),
	}
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
			Scanner = MakeCppScanner(env, fn),
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

		if type(src) == "table" then
			assert(depgraph.IsNode(src))
			local outFiles = depgraph.SpliceOutputsSingle(src, suffixes)
			util.AppendTable(inputs, outFiles)
			table.insert(deps, src)
		else
			table.insert(inputs, src)
		end
	end

	return inputs, deps
end

local function LinkCommon(env, args, label, action, suffix)
	local function obj_hook(fn)
		return env.Make.Object { Source = fn, Pass = args.Pass }
	end
	local inputs, deps = AnalyzeSources(args.Sources, { env:Get("OBJECTSUFFIX") }, obj_hook)
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

DefaultEnvironment.Make.Library = function (env, args)
	return LinkCommon(env, args, "Library", "$(LIBCOM)", "$(LIBSUFFIX)")
end

DefaultEnvironment.Make.Program = function (env, args)
	return LinkCommon(env, args, "Program", "$(PROGCOM)", "$(PROGSUFFIX)")
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

