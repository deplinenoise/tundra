
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
		{ Name="Quiet", Short="q", Long="quiet", Doc="Don't print actions as they execute" },
		{ Name="Continue", Short="k", Long="continue", Doc="Build as much as possible" },
		{ Name="Verbose", Short="v", Long="verbose", Doc="Be verbose" },
		{ Name="VeryVerbose", Short="w", Long="very-verbose", Doc="Be very verbose" },
		{ Name="DryRun", Short="n", Long="dry-run", Doc="Don't execute any actions" },
		{ Name="ThreadCount", Short="j", Long="threads", Doc="Specify number of build threads", HasValue=true },
		{ Name="IdeGeneration", Short="g", Long="ide-gen", "Generate IDE integration files for the specified IDE", HasValue=true },
		{ Name="AllConfigs", Short="a", Long="all-configs", Doc="Build all configurations at once (useful in IDE mode)" },
		{ Name="DebugQueue", Long="debug-queue", Doc="Show build queue debug information" },
		{ Name="DebugNodes", Long="debug-nodes", Doc="Show DAG node debug information" },
		{ Name="DebugAncestors", Long="debug-ancestors", Doc="Show ancestor debug information" },
		{ Name="DebugStats", Long="debug-stats", Doc="Show statistics on the build session" },
		{ Name="DebugReason", Long="debug-reason", Doc="Show build reasons" },
		{ Name="DebugScan", Long="debug-scan", Doc="Show dependency scanner debug information" },
		{ Name="SelfTest", Long="self-test", Doc="Run a test of Tundra's internals" },
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
		Options.Verbosity = 3
		Options.Verbose = true
	elseif Options.Verbose then
		Options.Verbosity = 2
	elseif Options.Quiet then
		Options.Verbosity = 0
	else
		Options.Verbosity = 1 -- default
	end

	if Options.Help then
		io.write("Tundra Build Processor v0.5.0\n")
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

	if Options.SelfTest then
		dofile(TundraRootDir .. "/scripts/selftest.lua")
		native.exit(0)
	end
end

function match_build_id(id, default)
	assert(id)
	local i = id:gmatch("[^-]+")
	local platform_name, toolset, variant, subvariant = i() or default, i() or default, i() or default, i() or default
	if not platform_name or not toolset then
		errorf("%s doesn't follow <platform>-<toolset>-[<variant>-[<subvariant>]] convention", id)
	end
	return platform_name, toolset, variant, subvariant
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
	DebugFlags = Options.DebugFlags,
	Verbosity = Options.Verbosity,
	ThreadCount = tonumber(Options.ThreadCount),
	DryRun = Options.DryRun and 1 or 0,
	UseDigestSigning = 0,
	ContinueOnError = Options.Continue and 1 or 0,
}


local function run_build_script(fn)
	local script_globals, script_globals_mt = {}, {}
	script_globals_mt.__index = _G
	setmetatable(script_globals, script_globals_mt)

	local chunk, error_msg = loadfile(fn)
	if not chunk then
		io.stderr:write(error_msg .. '\n')
		native.exit()
	end
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

local loaded_toolsets = {}
function load_toolset(id, env)
	local chunk = loaded_toolsets[id] 
	if not chunk then
		local path = TundraRootDir .. "/scripts/tools/" .. id ..".lua"
		if Options.Verbose then
			print("loading toolset " .. id .. " from " .. path) 
		end
		chunk = assert(loadfile(path))
		loaded_toolsets[id] = chunk
	end
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

local function analyze_targets(targets, configs, variants, subvariants, default_variant, default_subvariant)
	local build_tuples = {}
	local remaining_targets = {}

	local build_configs = {}
	local build_variants = {}
	local build_subvariants = {}

	if not Options.AllConfigs then
		for _, name in ipairs(targets) do
			name = name
			if configs[name] then
				build_configs[#build_configs + 1] = configs[name]
			elseif variants[name] then
				build_variants[#build_variants + 1] = name
			elseif subvariants[name] then
				build_subvariants[#build_subvariants + 1] = name
			else
				local config, toolset, variant, subvariant = match_build_id(name)
				config = config .. "-".. toolset
				if config and variant then
					if not configs[config] then
						local config_names = {}
						for name, _ in pairs(configs) do config_names[#config_names + 1] = name end
						errorf("config %s is not supported; specify one of %s", config, table.concat(config_names, ", "))
					end
					if not variants[variant] then
						errorf("variant %s is not supported; specify one of %s", variant, table.concat(variants, ", "))
					end

					if subvariant then
						if not subvariants[subvariant] then
							errorf("subvariant %s is not supported; specify one of %s", subvariant, table.concat(subvariants, ", "))
						end
					else
						subvariant = default_subvariant
					end

					build_tuples[#build_tuples + 1] = { Config = configs[config], Variant = variant, SubVariant = subvariant }
				else
					remaining_targets[#remaining_targets + 1] = name
				end
			end
		end

		-- If no configurations have been specified, default to the ones that are
		-- marked DefaultOnHost for the current host platform.
		if #build_configs == 0 and #build_tuples == 0 then
			local host_os = native.host_platform
			for name, config in pairs(configs) do
				if config.DefaultOnHost == host_os then
					if Options.VeryVerbose then
						if Options.VeryVerbose then
							printf("defaulted to %s based on host platform %s..", name, host_os)
						end
					end
					build_configs[#build_configs + 1] = config
				end
			end
		end
	else
		-- User has requested all configurations at once. Possibly due to IDE mode.
		for _, cfg in pairs(configs) do build_configs[#build_configs + 1] = cfg end
		for var, _ in pairs(variants) do build_variants[#build_variants + 1] = var end
		for var, _ in pairs(subvariants) do build_subvariants[#build_subvariants + 1] = var end
	end

	-- If no variants have been specified, use the default variant.
	if #build_variants == 0 then
		build_variants = { default_variant }
	end
	if #build_subvariants == 0 then
		build_subvariants = { default_subvariant }
	end

	for _, config in ipairs(build_configs) do
		for _, variant in ipairs(build_variants) do
			for _, subvariant in ipairs(build_subvariants) do
				build_tuples[#build_tuples + 1] = { Config = config, Variant = variant, SubVariant = subvariant }
			end
		end
	end

	if #build_tuples == 0 then
		io.stderr:write("no build tuples available and no host-default configs defined -- choose one of\n")
		for _, config in pairs(configs) do
			for variant, _ in pairs(variants) do
				io.stderr:write(string.format("  %s-%s\n", config.Name, variant))
			end
		end
		native.exit()
	end

	return remaining_targets, build_tuples
end

local default_variants = { "debug", "production", "release" }
local default_subvariants = { "default" }

local function iter_inherits(config, name)
	local tab = config
	return function()
		while tab do
			local my_tab = tab
			if not my_tab then break end
			tab = my_tab.Inherit
			local v = my_tab[name]
			if v then return v end
		end
	end
end

local function setup_env(env, tuple)
	local config = tuple.Config
	local variant_name = tuple.Variant
	local build_id = config.Name .. "-" .. variant_name .. "-" .. tuple.SubVariant
	local naked_platform, naked_toolset = match_build_id(build_id)

	if Options.Verbose then
		printf("configuring for %s", build_id)
	end

	env:set("CURRENT_PLATFORM", naked_platform) -- e.g. linux or macosx
	env:set("CURRENT_TOOLSET", naked_toolset) -- e.g. gcc or msvc
	env:set("CURRENT_VARIANT", tuple.Variant) -- e.g. debug or release
	env:set("BUILD_ID", build_id) -- e.g. linux-gcc-debug
	env:set("OBJECTDIR", "$(OBJECTROOT)" .. SEP .. "$(BUILD_ID)")

	for tools in iter_inherits(config, "Tools") do
		for _, toolset_name in ipairs(tools) do
			load_toolset(toolset_name, env)
		end
	end

	for env_tab in iter_inherits(config, "Env") do
		for key, val in util.pairs(env_tab) do
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
	end
end

function Build(args)
	local passes = args.Passes or { { Name = "Default", BuildOrder = 1 } }

	if type(args.Configs) ~= "table" or #args.Configs == 0 then
		error("Need at least one config; got " .. util.tostring(args.Configs) )
	end

	local configs, variants, subvariants = {}, {}, {}

	for _, cfg in ipairs(args.Configs) do
		configs[assert(cfg.Name)] = cfg
	end

	local variant_array = args.Variants or default_variants
	for _, variant in ipairs(variant_array) do variants[variant] = true end

	local subvariant_array = args.SubVariants or default_subvariants
	for _, subvariant in ipairs(subvariant_array) do subvariants[subvariant] = true end

	local default_variant = args.DefaultVariant or variant_array[1]
	local default_subvariant = args.DefaultSubVariant or subvariant_array[1]
	local named_targets, build_tuples = analyze_targets(Targets, configs, variants, subvariants, default_variant, default_subvariant)

	if not Options.IdeGeneration then
		-- This is a regular build. Assume these generator sets are always
		-- needed for now. Could possible make an option for which generator
		-- sets to load.
		nodegen.add_generator_set("nodegen", "native")
		nodegen.add_generator_set("nodegen", "dotnet")
	else
		-- We are generating IDE integration files. Load the specified
		-- integration module rather than DAG builders.
		nodegen.add_generator_set("ide", Options.IdeGeneration)
	end

	local d = decl.make_decl_env()
	do
		local platforms = {}
		for _, tuple in ipairs(build_tuples) do
			local platform_name, tools = match_build_id(tuple.Config.Name)
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

	if not Options.IdeGeneration then
		local everything = {}

		-- Let the nodegen code generate DAG nodes for all active
		-- configurations/variants.
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

		-- Unless we're just creating IDE integration files, create a top-level
		-- node and pass the full DAG to the build engine.
		local toplevel = default_env:make_node {
			Label = "toplevel",
			Dependencies = everything,
		}
		GlobalEngine:build(toplevel)

	else
		-- We're just generating IDE files. Pass the build tuples directly to
		-- the generator and let it write files.
		local env = default_env:clone()
		nodegen.generate_ide_files(build_tuples, default_names, raw_nodes, env)
	end
end

run_build_script("tundra.lua")
