
-- Set up the package path based on the script path first thing so we can require() stuff.
local cmdline_args = ...
TundraRootDir = assert(cmdline_args[1])
do
	local package = require("package")
	package.path = string.format("%s/scripts/?.lua;%s/lua/etc/?.lua", TundraRootDir, TundraRootDir)
end

-- Use "strict" when developing to flag accesses to nil global variables
require "strict"

function printf(msg, ...)
	local str = string.format(msg, ...)
	print(str)
end

function errorf(msg, ...)
	local str = string.format(msg, ...)
	error(str)
end

local util = require "tundra.util"
local environment = require "tundra.environment"
local native = require "tundra.native"
local nodegen = require "tundra.nodegen"
local decl = require "tundra.decl"

-- Parse the command line options.
do
	local message = nil
	local option_blueprints = {
		{ Name="Help", Short="h", Long="help", Doc="This message" },
		{ Name="Verbose", Short="v", Long="verbose", Doc="Be verbose" },
		{ Name="VeryVerbose", Short="w", Long="very-verbose", Doc="Be very verbose" },
		{ Name="DryRun", Short="n", Long="dry-run", Doc="Don't execute any actions" },
		{ Name="WriteGraph", Long="dep-graph", Doc="Generate dependency graph" },
		{ Name="GraphEnv", Long="dep-graph-env", Doc="Include environments in dependency graph" },
		{ Name="GraphFilename", Long="dep-graph-filename", Doc="Dependency graph filename", HasValue=true },
		{ Name="SelfTest", Long="self-test", Doc="Perform self-tests" },
		{ Name="ThreadCount", Short="j", Long="threads", Doc="Specify number of build threads", HasValue=true },
		{ Name="DebugQueue", Long="debug-queue", Doc="Show build queue debug information" },
		{ Name="DebugNodes", Long="debug-nodes", Doc="Show DAG node debug information" },
		{ Name="DebugAncestors", Long="debug-ancestors", Doc="Show ancestor debug information" },
		{ Name="DebugStats", Long="debug-stats", Doc="Show statistics on the build session" },
		{ Name="DebugReason", Long="debug-reason", Doc="Show build reasons" },
		{ Name="DebugScan", Long="debug-scan", Doc="Show dependency scanner debug information" },
	}
	Options, Targets, message = util.parse_cmdline(cmdline_args, option_blueprints)
	if message then
		io.write(message)
		return 1
	end

	Options.DebugFlags = 0
	do
		local flag_values = {
			[1] = Options.DebugQueue,
			[2] = Options.DebugNodes,
			[4] = Options.DebugAncestors,
			[8] = Options.DebugStats,
			[16] = Options.DebugReason,
			[32] = Options.DebugScan,
		}

		for k, v in pairs(flag_values) do
			if v then
				Options.DebugFlags = Options.DebugFlags + k
			end
		end
	end

	if Options.VeryVerbose then
		Options.Verbosity = 2
		Options.Verbose = true
	elseif Options.Verbose then
		Options.Verbosity = 1
	else
		Options.Verbosity = 0
	end

	if Options.Help then
		io.write("Tundra Build Processor v0.2.0\n")
		io.write("Copyright (c)2010 Andreas Fredriksson. All rights reserved.\n\n")
		io.write("Command-line options:\n")
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


if Options.VeryVerbose then
	print("Options:")
	for k, v in pairs(Options) do
		print(k .. ": " .. util.tostring(v))
	end
end

SEP = native.host_platform == "windows" and "\\" or "/"

local default_env = environment.create()
default_env:set_many {
	["OBJECTROOT"] = "tundra-output",
	["SEP"] = SEP,
}

GlobalEngine = native.make_engine {
	FileHashSize = 51921,
	RelationHashSize = 79127,
	BuildId = "rev6",
	DebugFlags = Options.DebugFlags,
	Verbosity = Options.Verbosity,
	ThreadCount = tonumber(Options.ThreadCount),
	DryRun = Options.DryRun and 1 or 0,
}


local function run_build_script(fn)
	local script_globals, script_globals_mt = {}, {}
	script_globals_mt.__index = _G
	setmetatable(script_globals, script_globals_mt)

	local chunk = assert(loadfile(fn))
	setfenv(chunk, script_globals)

	local function stack_dumper(err_obj)
		local debug = require "debug"
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


do
	local host_script = TundraRootDir .. "/scripts/host/" .. native.host_platform .. ".lua"
	if Options.Verbose then
		print("loading host settings from " .. host_script) 
	end
	local chunk = assert(loadfile(host_script))
	chunk(default_env)
end

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

function load_toolset(id, env)
	local path = TundraRootDir .. "/scripts/tools/" .. id ..".lua"
	if Options.Verbose then
		print("loading toolset " .. id .. " from " .. path) 
	end
	local chunk = assert(loadfile(path))
	chunk(env)
end

local function member(list, item)
	assert(type(list) == "table")
	for _, x in ipairs(list) do
		if x == item then
			return true
		end
	end
	return false
end

local function analyze_targets(targets, configs, variants)
	local build_tuples = {}
	local remaining_targets = {}

	local build_configs = {}
	local build_variants = {}

	for _, name in ipairs(targets) do
		name = name
		if configs[name] then
			build_configs[#build_configs + 1] = configs[name]
		elseif variants[name] then
			build_variants[#build_variants + 1] = name
		else
			local config, variant = string.match(name, "^(%w+-%w+)(%w+)$")
			if config and variant then
				if not configs[config] then
					local config_names = map(configs, function (x) return x.Name end)
					errorf("config %s is not supported; specify one of %s", config, table.concat(config_names, ", "))
				end
				if not variants[variant] then
					errorf("variant %s is not supported; specify one of %s", variant, table.concat(variants, ", "))
				end
				build_tuples[#build_tuples + 1] = { configs[config], variant }
			else
				remaining_targets[#remaining_targets + 1] = name
			end
		end
	end

	for _, config in ipairs(build_configs) do
		for _, variant in ipairs(build_variants) do
			build_tuples[#build_tuples + 1] = { Config = config, Variant = variant }
		end
	end

	if #build_tuples == 0 then
		io.stderr:write("no build tuples available -- chose one of\n")
		for _, config in pairs(configs) do
			for variant, _ in pairs(variants) do
				io.stderr:write(string.format("  %s-%s\n", config.Name, variant))
			end
		end
		error("giving up")
	end

	return remaining_targets, build_tuples
end

local default_variants = { "debug", "production", "release" }

local function setup_env(env, tuple)
	local config = tuple.Config
	local variant_name = tuple.Variant
	local build_id = config.Name .. '-' .. variant_name
	local naked_platform, naked_toolset = string.match(config.Name, "^(%w+)-(%w+)$")

	if Options.Verbose then
		printf("configuring for %s", build_id)
	end

	env:set("CURRENT_PLATFORM", naked_platform) -- e.g. linux or macosx
	env:set("CURRENT_TOOLSET", naked_toolset) -- e.g. gcc or msvc
	env:set("CURRENT_VARIANT", tuple.Variant) -- e.g. debug or release
	env:set("BUILD_ID", build_id) -- e.g. linux-gcc-debug
	env:set("OBJECTDIR", "$(OBJECTROOT)" .. SEP .. "$(BUILD_ID)")

	for _, toolset_name in util.nil_ipairs(config.Tools) do
		load_toolset(toolset_name, env)
	end

	local tab = config
	while tab do
		for key, val in util.nil_pairs(tab.Env) do
			if Options.VeryVerbose then
				printf("env append %s = %s", key, util.tostring(val))
			end
			if type(val) == "table" then
				for _, subvalue in ipairs(val) do
					env:append(key, subvalue)
				end
			else
				env:append(key, val)
			end
		end
		tab = tab.Inherit
		if tab and Options.VeryVerbose then
			print("--inherted data--")
		end
	end
end

function Build(args)
	local passes = args.Passes or { { Name = "Default", BuildOrder = 1 } }

	if type(args.Configs) ~= "table" or #args.Configs == 0 then
		error("Need at least one config; got " .. util.tostring(configs) )
	end

	local configs, variants = {}, {}

	for _, cfg in ipairs(args.Configs) do
		configs[assert(cfg.Name)] = cfg
	end

	for _, variant in ipairs(args.Variants or default_variants) do
		variants[variant] = true
	end

	local named_targets, build_tuples = analyze_targets(Targets, configs, variants)

	-- Assume these are always needed for now. Could possible make an option
	-- for which generator sets to load.
	nodegen.add_generator_set("native")
	nodegen.add_generator_set("dotnet")

	local d = decl.make_decl_env()
	do
		local platforms = {}
		for _, tuple in ipairs(build_tuples) do
			local platform_name, tools = string.match(tuple.Config.Name, "^(%w+)-(%w+)$")
			if not platform_name then
				errorf("config %s doesn't follow <platform>-<toolset> convention", tuple.Config.Name)
			end
			if not member(platforms, platform_name) then
				platforms[#platforms + 1] = platform_name
			end
		end
		if Options.Verbose then
			printf("adding platforms: %s", table.concat(platforms, ", "))
		end
		d:add_platforms(platforms)
	end

	for _, fn in util.nil_ipairs(args.AdditionalParseFiles) do
		if Options.Verbose then
			printf("parsing user-defined declaration parsers from %s", fn)
		end
		local chunk = assert(loadfile("codegen.lua"))
		chunk(d, passes)
	end

	local raw_nodes, default_names = d:parse(args.Units or "units.lua")
	assert(#default_names > 0, "no default unit name to build was set")

	local everything = {}

	for _, tuple in pairs(build_tuples) do
		local env = default_env:clone()
		setup_env(env, tuple, configs)
		everything[#everything + 1] = nodegen.generate {
			Engine = GlobalEngine,
			Env = env,
			Config = tuple.Config.Name,
			Variant = tuple.Variant,
			Declarations = raw_nodes,
			DefaultNames = default_names,
			Passes = passes,
		}
	end

	local toplevel = default_env:make_node {
		Label = "toplevel",
		Dependencies = everything,
	}
	GlobalEngine:build(toplevel)
end

run_build_script("tundra.lua")
