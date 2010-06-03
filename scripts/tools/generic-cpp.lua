local depgraph = require("tundra.depgraph")
local util = require("tundra.util")
local path = require("tundra.path")
local engine = require("tundra.native.engine")

DefaultEnvironment.Make.HeaderScan = function(env, args)
	return env:MakeNode {
		Label = "Scan headers for $(<)",
		Cachable = true,
		Type = depgraph.NodeType.GraphGenerator,
		Action = "lua ScanHeaders",
		InputFiles = assert(args.Sources),
		Generated = args.Generated,
	}
end

DefaultEnvironment.Make.CppObject = function(env, args)
	local function GetObjectFilename(fn)
		return '$(OBJECTDIR)/' .. path.GetFilenameBase(fn) .. '$(OBJECTSUFFIX)'
	end
	assert(type(args) == "table", "CppObject expects single c++ source file")
	local fn = assert(args[1], "Expected C++ source file")
	assert(type(fn) == "string", "Argument must be a string")
	local object_fn = GetObjectFilename(fn)
	local node = env:MakeNode {
		Label = 'CppCompile $(@)',
		Action = "$(CPPCOM)",
		InputFiles = { fn },
		OutputFiles = { object_fn },
		Dependencies = { env.Make.HeaderScan { Sources = { fn } } }
	}
	return node
end

DefaultEnvironment.Make.Object = function(env, args)
	assert(type(args) == "table" and #args == 1, "Object expects a single source node")
	local input = args[1]

	-- Allow premade objects to be passed here to e.g. Library's Sources list
	if type(input) == "table" then
		return input
	end

	local ext = path.GetExtension(input)
	for _, cppext in ipairs(env:GetList('CPPEXT', { 'cpp', 'cc' })) do
		if cppext == ext then
			return env.Make.CppObject { input }
		end
	end

	error (input .. ": unsupported suffix " .. ext)
end

DefaultEnvironment.Make.Library = function (env, args)
	assert(env, "No environment")
	assert(args, "No args")
	local name = util.GetNamedArg(args, "Name")
	local sources = util.GetNamedArg(args, "Sources")
	local objects = util.map(sources, function(fn) return env.Make.Object { fn } end)
	local libnode = env:MakeNode {
		Label = "Library $(@)",
		Action = "$(LIBCOM)",
		Dependencies = objects,
		InputFiles = depgraph.SpliceOutputs(objects),
		OutputFiles = { name .. '$(LIBSUFFIX)' }
	}
	return libnode
end

function ScanHeaders(node)
	local input_files = assert(node:GetInputFiles())
	local verbose = Options.Verbose and Options.Verbose > 1
	for _, fn in ipairs(input_files) do
		local dirname = select(1, path.SplitPath(fn))
		local line_number = 1
		local f = assert(io.open(fn, 'r'))
		for line in f:lines() do
			local path_expr = line:match('^%s*#%s*include%s*(["<].*[">])')
			if path_expr then
				local path = path_expr:sub(2, -2) -- drop quotes/angles
				local paths_to_search, found
				if path_expr:sub(1, 1) == '"' then
					paths_to_search = { dirname }
				else
					local sys_list = env:GetList('SYSCPPPATH', {})
					local cpp_list = env:GetList('CPPPATH', {})
					assert(type(sys_list) == "table")
					assert(type(cpp_list) == "table")
					paths_to_search = util.MergeArrays(sys_list, cpp_list)
				end

				for _, include_path in ipairs(paths_to_search) do
					local full_path = include_path .. "/" .. path
					local size, digest = engine.StatPath(full_path)
					if size >= 0 then
						node:AddDependency(env.Make.HeaderScan { Sources = { full_path }, Generated = true })
						found = true
						break
					end
				end

				if not found then
					if verbose then
						local msg = string.format("%s(%d): can't find %s in any of these include paths: [%s]", fn, line_number, path, table.concat(paths_to_search, ", "))
						io.stderr:write(msg, "\n")
					end
				end

			end
			line_number = line_number + 1
		end
		f:close()

	end
end

