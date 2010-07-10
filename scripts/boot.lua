
-- Set up the package path based on the script path first thing so we can require() stuff.
local cmdline_args = ...
TundraRootDir = assert(cmdline_args[1])
do
	local package = require("package")
	package.path = string.format("%s/scripts/?.lua;%s/lua/etc/?.lua", TundraRootDir, TundraRootDir)
end

function printf(msg, ...)
	local str = string.format(msg, ...)
	print(str)
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
	Options, Targets, message = util.parse_cmdline(cmdline_args, option_blueprints)
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

local environment = require("tundra.environment")

if Options.Verbose then
	print("Options:")
	for k, v in pairs(Options) do
		print(k, v)
	end
	print("Targets:", table.concat(Targets, ", "))
end

DefaultEnvironment = environment.create()
DefaultEnvironment:set_many {
	["OBJECTDIR"] = "tundra-output",
}

function run_build_script(fn)
	local script_globals, script_globals_mt = {}, {}
	script_globals_mt.__index = _G
	setmetatable(script_globals, script_globals_mt)

	local chunk = assert(loadfile(fn))
	setfenv(chunk, script_globals)

	local function stack_dumper(err_obj)
		local debug = require("debug")
		return debug.traceback(err_obj, 2)
	end

	local function args_stub()
		return chunk()
	end

	local success, result = xpcall(args_stub, stack_dumper)

	if not success then
		io.stderr:write(result)
		error("failure")
	else
		return result
	end
end

local native = require("tundra.native")

do
	local host_script = TundraRootDir .. "/scripts/host/" .. native.host_platform .. ".lua"
	if Options.Verbose then
		print("loading host settings from " .. host_script) 
	end
	local chunk = assert(loadfile(host_script))
	chunk(DefaultEnvironment)
end

native_engine = native.make_engine {
	FileHashSize = 51921,
	RelationHashSize = 79127,
	BuildId = "test-build",
}

SEP = native.host_platform == "windows" and "\\" or "/"

function glob(directory, pattern)
	local result = {}
	for dir, dirs, files in native.walk_path(directory) do
		util.filter_in_place(files, function (val) return string.match(val, pattern) end)
		for _, fn in ipairs(files) do
			result[#result + 1] = dir .. SEP .. fn
		end
	end
	return result
end

local function print_tree(n, level)
	if not level then level = 0 end
	local indent = string.rep("    ", level)
	printf("%s=> %s [pass: %s]", indent, n:get_annotation(), n.pass.Name)
	printf("%scmd: %s", indent, n:get_action())
	for _, fn in util.nil_ipairs(n:get_input_files()) do printf("%s   [ input: %s ]", indent, fn) end
	for _, fn in util.nil_ipairs(n:get_output_files()) do printf("%s   [ output: %s ]", indent, fn) end
	for _, dep in util.nil_ipairs(n:get_dependencies()) do
		print_tree(dep, level + 1)
	end
end

function build(node)
	--if Options.Verbose then
	--	PrintTree(node)
	--end
	native_engine:build(node)
end

function load_toolset(id, env)
	local path = TundraRootDir .. "/scripts/tools/" .. id ..".lua"
	if Options.Verbose then
		print("loading toolset " .. id .. " from " .. path) 
	end
	local chunk = assert(loadfile(path))
	chunk(env)
end

run_build_script("tundra.lua")
