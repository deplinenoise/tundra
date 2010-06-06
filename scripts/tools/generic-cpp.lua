local depgraph = require("tundra.depgraph")
local util = require("tundra.util")
local path = require("tundra.path")

do
	local cc_compile = function(env, args)
		local function GetObjectFilename(fn)
			return '$(OBJECTDIR)/' .. path.GetFilenameBase(fn) .. '$(OBJECTSUFFIX)'
		end
		assert(type(args) == "table", "CppObject expects single c++ source file")
		local fn = assert(args[1], "Expected C++ source file")
		assert(type(fn) == "string", "Argument must be a string")
		local object_fn = GetObjectFilename(fn)
		local node = env:MakeNode {
			Label = 'Cc $(@)',
			Action = "$(CCCOM)",
			InputFiles = { fn },
			OutputFiles = { object_fn },
		}
		return node
	end

	DefaultEnvironment.Make.CcObject = cc_compile
	DefaultEnvironment:RegisterImplicitMakeFn("c", cc_compile)
end

DefaultEnvironment.Make.Object = function(env, args)
	assert(type(args) == "table" and #args == 1, "Object expects a single source node")
	local input = args[1]

	-- Allow premade objects to be passed here to e.g. Library's Sources list
	if type(input) == "table" then
		return input
	end

	local implicitMake = env:GetImplicitMakeFn(input)
	return implicitMake(env, args)
end

DefaultEnvironment.Make.Library = function (env, args)
	assert(env, "No environment")
	assert(args, "No args")
	local name = util.GetNamedArg(args, "Name")
	local sources = util.GetNamedArg(args, "Sources")
	local objects = util.map(sources, function(fn) return env.Make.Object { fn } end)
	assert(#sources == #objects)
	local libnode = env:MakeNode {
		Label = "Library $(@)",
		Action = "$(LIBCOM)",
		Dependencies = objects,
		InputFiles = depgraph.SpliceOutputs(objects, { env:Get("OBJECTSUFFIX") }),
		OutputFiles = { name .. '$(LIBSUFFIX)' }
	}
	return libnode
end

DefaultEnvironment.Make.Program = function (env, args)
	assert(env, "No environment")
	assert(args, "No args")
	local name = util.GetNamedArg(args, "Name")
	local sources = util.GetNamedArg(args, "Sources")
	if #sources < 1 then
		error(string.format("no sources for program %s provided", name))
	end
	local objects = util.map(sources, function(fn) return env.Make.Object { fn } end)
	assert(#sources == #objects)
	return env:MakeNode {
		Label = "Program $(@)",
		Action = "$(PROGCOM)",
		Dependencies = objects,
		InputFiles = depgraph.SpliceOutputs(objects, { env:Get("OBJECTSUFFIX"), env:Get("LIBSUFFIX") }),
		OutputFiles = { name .. '$(PROGSUFFIX)' }
	}
end

