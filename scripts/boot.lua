-- Copyright 2010 Andreas Fredriksson
--
-- This file is part of Tundra.
--
-- Tundra is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- Tundra is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with Tundra.  If not, see <http://www.gnu.org/licenses/>.

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
local native = require "tundra.native"

-- Parse the command line options.
do
	local message = nil
	local option_blueprints = {
		{ Name="Help", Short="h", Long="help", Doc="This message" },
		{ Name="Version", Short="V", Long="version", Doc="Display version info" },
		{ Name="Clean", Short="c", Long="clean", Doc="Remove output files (clean)" },
		{ Name="Quiet", Short="q", Long="quiet", Doc="Don't print actions as they execute" },
		{ Name="Continue", Short="k", Long="continue", Doc="Build as much as possible" },
		{ Name="Verbose", Short="v", Long="verbose", Doc="Be verbose" },
		{ Name="VeryVerbose", Short="w", Long="very-verbose", Doc="Be very verbose" },
		{ Name="DryRun", Short="n", Long="dry-run", Doc="Don't execute any actions" },
		{ Name="ThreadCount", Short="j", Long="threads", Doc="Specify number of build threads", HasValue=true },
		{ Name="IdeGeneration", Short="g", Long="ide-gen", "Generate IDE integration files for the specified IDE", HasValue=true },
		{ Name="AllConfigs", Short="a", Long="all-configs", Doc="Build all configurations at once (useful in IDE mode)" },
		{ Name="Cwd", Short="C", Long="set-cwd", Doc="Set the current directory before building", HasValue=true },
		{ Name="DebugQueue", Long="debug-queue", Doc="Show build queue debug information" },
		{ Name="DebugNodes", Long="debug-nodes", Doc="Show DAG node debug information" },
		{ Name="DebugAncestors", Long="debug-ancestors", Doc="Show ancestor debug information" },
		{ Name="DebugStats", Long="debug-stats", Doc="Show statistics on the build session" },
		{ Name="DebugReason", Long="debug-reason", Doc="Show build reasons" },
		{ Name="DebugScan", Long="debug-scan", Doc="Show dependency scanner debug information" },
		{ Name="Debugger", Long="lua-debugger", Doc="Run with Lua debugger enabled (use 'exit' to continue)" },
		{ Name="SelfTest", Long="self-test", Doc="Run a test of Tundra's internals" },
	}
	Options, Targets, message = util.parse_cmdline(cmdline_args, option_blueprints)
	if message then
		io.write(message, "\n")
		io.write("Try `tundra -h' to display an option summary\n")
		return 1
	end

	if Options.Debugger then
		require "debugger"
		pause()
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

	if Options.Version or Options.Help then
		io.write("Tundra Build Processor v0.5.1\n")
		io.write("Copyright (C) 2010 Andreas Fredriksson\n\n")
		io.write("This program comes with ABSOLUTELY NO WARRANTY.\n")
		io.write("This is free software, and you are welcome to redistribute it\n")
		io.write("under certain conditions; see the GNU GPL license for details.\n")
	end

	if Options.Help then
		io.write("\nCommand-line options:\n")
		for _, bp in ipairs(option_blueprints) do
			local l = string.format("  %- 3s %- 25s %s\n",
			bp.Short and "-"..bp.Short or "",
			bp.Long and "--"..bp.Long or "",
			bp.Doc or "")
			io.write(l)
		end
	end

	if Options.Version or Options.Help then
		return 0
	end

	if Options.SelfTest then
		dofile(TundraRootDir .. "/scripts/selftest.lua")
		native.exit(0)
	end
end

local environment = require "tundra.environment"
local nodegen = require "tundra.nodegen"
local decl = require "tundra.decl"
local path = require "tundra.path"

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

if Options.Cwd then
	if Options.VeryVerbose then
		print("changing to dir \"" .. Options.Cwd .. "\"")
	end
	native.set_cwd(Options.Cwd)
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

local syntax_dirs = { TundraRootDir .. "/scripts/syntax/" }
local toolset_dirs = { TundraRootDir .. "/scripts/tools/" }
local loaded_toolsets = {}
local loaded_syntaxes = {}
local toolset_once_map = {}

local function get_memoized_chunk(kind, id, table, dirs)
	local chunk = table[id]
	if chunk then return chunk end

	for _, dir in ipairs(dirs) do
		if dir:len() > 0 then
			dir = dir .. '/'
		end
		local path = dir .. id ..".lua"
		local f, err = io.open(path, 'r')
		if f then
			if Options.Verbose then
				print("loading toolset " .. id .. " from " .. path)
			end
			local data = f:read("*a")
			f:close()
			chunk = assert(loadstring(data, path))
			table[id] = chunk
			return chunk
		end
	end

	errorf("couldn't find %s %s in any of these paths: %s", kind, id, util.tostring(dirs))
end

function load_toolset(id, env, options)
	local chunk = get_memoized_chunk("toolset", id, loaded_toolsets, toolset_dirs)
	chunk(env, options)
end

function load_syntax(id, decl, passes)
	local chunk = get_memoized_chunk("syntax", id, loaded_syntaxes, syntax_dirs)
	chunk(decl, passes)
end

function toolset_once(id, fn)
	local v = toolset_once_map[id]
	if not v then
		v = fn()
		toolset_once_map[id] = v
	end
	return v
end

local function add_toolset_dir(dir)
	if Options.VeryVerbose then
		printf("adding toolset dir \"%s\"", dir)
	end
	-- Make sure dir is sane
	dir = path.normalize(dir)
	-- Add user toolset dir first so they can override builtin scripts.
	table.insert(toolset_dirs, 1, dir)
end

local function add_syntax_dir(dir)
	if Options.VeryVerbose then
		printf("adding syntax dir \"%s\"", dir)
	end
	-- Make sure dir is sane and ends with a slash
	dir = path.normalize(dir)
	-- Add user toolset dir first so they can override builtin scripts.
	table.insert(syntax_dirs, 1, dir)
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
		for _, data in ipairs(tools) do
			local id, options
			if type(data) == "string" then
				id = data
			elseif type(data) == "table" then
				id = assert(data[1])
				options = data
			else
				error("bad parameters")
			end
			load_toolset(id, env, options)
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

	for _, dir in util.nil_ipairs(args.ToolsetDirs) do
		add_toolset_dir(dir)
	end

	for _, dir in util.nil_ipairs(args.SyntaxDirs) do
		add_syntax_dir(dir)
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

	for _, id in util.nil_ipairs(args.SyntaxExtensions) do
		if Options.Verbose then
			printf("parsing user-defined declaration parsers from %s", id)
		end
		load_syntax(id, d, passes)
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
		if Options.Clean then
			GlobalEngine:build(toplevel, "clean")
		else
			GlobalEngine:build(toplevel)
		end

	else
		-- We're just generating IDE files. Pass the build tuples directly to
		-- the generator and let it write files.
		local env = default_env:clone()
		nodegen.generate_ide_files(build_tuples, default_names, raw_nodes, env)
	end
end

run_build_script("tundra.lua")
