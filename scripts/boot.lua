
-- Set up the package path based on the script path first thing so we can require() stuff.
local cmdline_args = ...
TundraRootDir = assert(cmdline_args[1])
do
	local package = require("package")
	package.path = string.format("%s/scripts/?.lua;%s/lua/etc/?.lua", TundraRootDir, TundraRootDir)
end

-- Use "strict" when developing to flag accesses to nil global variables
require("strict")

local util = require("tundra.util")

-- Parse the command line options.
do
	local message = nil
	local option_blueprints = {
		{ Name="Help", Short="h", Long="help", Doc="This message" },
		{ Name="Verbose", Short="v", Long="verbose", Doc="Be verbose" },
		{ Name="DryRun", Short="n", Long="dry-run", Doc="Don't execute any actions" },
		{ Name="WriteGraph", Long="dep-graph", Doc="Generate dependency graph" },
		{ Name="GraphEnv", Long="dep-graph-env", Doc="Include environments in dependency graph" },
		{ Name="GraphFilename", Long="dep-graph-filename", Doc="Dependency graph filename", HasValue=true },
		{ Name="SelfTest", Long="self-test", Doc="Perform self-tests" },
	}
	Options, Targets, message = util.ParseCommandline(cmdline_args, option_blueprints)
	if message then
		io.write(message)
		return 1
	end

	if #Targets == 0 then
		table.insert(Targets, "All")
	end

	if Options.Help then
		io.write("Tundra Build Processor, v0.0.5\n\nCommand-line options:\n")
		for _, bp in ipairs(option_blueprints) do
			local l = string.format("  %- 3s %- 25s %s\n",
			bp.Short and "-"..bp.Short or "",
			bp.Long and "--"..bp.Long or "",
			bp.Doc or "")
			io.write(l)
		end
		return 0
	end
end

local engine = require("tundra.native.engine")
engine.Init()

local environment = require("tundra.environment")
local depgraph = require("tundra.depgraph")
local make = require("tundra.make")

if Options.SelfTest then
	local fail_count = dofile(TundraRootDir .. "/scripts/selftest.lua")
	if fail_count > 0 then
		return 1
	else
		return 0
	end
end

if Options.Verbose then
	print("Options:")
	for k, v in pairs(Options) do
		print(k, v)
	end
	print("Targets:", table.concat(Targets, ", "))
end

DefaultEnvironment = environment.Create()
DefaultEnvironment:SetMany {
	["RM"] = "rm -f",
	["RMCOM"] = "$(RM) $(<)",
	["LIBPATH"] = "",
	["LIBCOM"] = "$(LIB) $(LIBFLAGS) $(@) $(<)",
	["LIB"] = "ar",
	["LIBFLAGS"] = "-ru",
	["LIBSUFFIX"] = ".a",
	["OBJECTDIR"] = ".",
	["OBJECTSUFFIX"] = ".o",
	["CC"] = "gcc",
	["CFLAGS"] = "",
	["C++FLAGS"] = "",
	["C++"] = "g++",
	["CCCOM"] = "$(CC) $(CFLAGS) -c -o $(@) $(<)",
	["CPPCOM"] = "$(C++) $(C++FLAGS) -c -o $(@) $(<)",
}

-- Initialize tools
do
	local chunk = loadfile(TundraRootDir .. "/scripts/tools.lua")
	chunk()
end

function RunBuildScript(node)
	local script_globals, script_globals_mt = {}, {}
	script_globals_mt.__index = _G
	setmetatable(script_globals, script_globals_mt)

	local inputs = node:GetInputFiles()
	if not inputs or #inputs ~= 1 then
		error(node:GetAnnotation() .. ": illegal number of inputs")
	end

	local fn = inputs[1]
	local chunk = assert(loadfile(fn))
	setfenv(chunk, script_globals)

	local function stack_dumper(err_obj)
		local debug = require("debug")
		return debug.traceback(err_obj, 2)
	end

	local function args_stub()
		return chunk(node)
	end

	local success, result = xpcall(args_stub, stack_dumper)

	if not success then
		io.stderr:write(result)
		error("failure")
	else
		return result
	end
end

--[[
-- Build initial dependency tree

local root = DefaultEnvironment:MakeNode {
	Type = depgraph.NodeType.GraphGenerator,
	Label = "Run tundra-root.lua",
	Action = "lua RunBuildScript",
	InputFiles = { "tundra-root.lua" },
}

-- Pull in old build signatures
make.ReadSignatureDb()

-- Pull in dependency graph cache
depgraph.ReadNodeCache("tundra-nodecache.db")

make.Make(root)

if not Options.DryRun then
	make.WriteSignatures()
	depgraph.WriteNodeCache("tundra-nodecache.db")
end

if false then
	local f = io.open("tundra.depgraph", "w")
	util.SerializeCycle(f, "All", All)
	f:close()
end

-- Dump debugging GraphViz output
if Options.WriteGraph then
	local fn = Options.GraphFilename or "depgraph.dot"
	depgraph.GenerateDotGraph(fn, { root })
end
--]]

-- Play with the new native graph builder

local g = require"tundra.native.graph"

local gb = g.New()
local n1 = gb:GetNode { Cacheable = true, Inputs = { "Foo", "Bar" }, Outputs = { "Apa" }, Action = "Foo", Annotation = "Teh annotation" }
local n2 = gb:GetNode { Cacheable = true, Inputs = { "Foo" }, Outputs = { "Bar" }, Action = "Snask" }
local n3 = gb:GetNode { Cacheable = true, Inputs = { "Bar" }, Outputs = { "Bar2" }, Action = "Snask2" }

print(n2:GetType(), n2:GetAction(), n2:GetId())

for f in n1:IterInputFiles() do
	print(f)
end

print(util.tostring(n1:GetInputFiles()))

for f in n1:IterOutputFiles() do
	print(f)
end

n1:AddDependency(n2, n3)

gb:Save("c:\\temp\\graphcache.txt")

local gb2 = g.New()
gb2:Load("c:\\temp\\graphcache.txt")
