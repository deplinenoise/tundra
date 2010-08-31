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

module(..., package.seeall)

-- Use "strict" when developing to flag accesses to nil global variables
require "strict"
local native = require "tundra.native"

local build_script_env = {}

-- Track accessed Lua files, for cache tracking. There's a Tundra-specific
-- callback that can be installed by calling set_loadfile_callback(). This
-- could improved if the Lua interpreted was changed to checksum files as they
-- were loaded and pass that hash here so we didn't have to open the files
-- again. OTOH, they should be in disk cache now.
local accessed_lua_files = {}
local function get_file_digest(fn)
	local f = assert(io.open(fn, 'rb'))
	local data = f:read("*all")
	f:close()
	return native.digest_guid(data)
end
local function record_lua_access(fn)
	if accessed_lua_files[fn] then return end
	accessed_lua_files[fn] = get_file_digest(fn)
end

set_loadfile_callback(record_lua_access)

-- Also patch 'dofile', 'loadfile' to track files loaded without the package
-- facility.
do
	local old = { "dofile", "loadfile" }
	for _, name in ipairs(old) do
		local func = _G[name]
		_G[name] = function(fn, ...)
			record_lua_access(fn)
			return func(fn, ...)
		end
	end
end

local util = require "tundra.util"

-- This trio is so useful we want them everywhere without imports.
function _G.printf(msg, ...)
	local str = string.format(msg, ...)
	print(str)
end

function _G.errorf(msg, ...)
	local str = string.format(msg, ...)
	error(str)
end

function _G.croak(msg, ...)
	local str = string.format(msg, ...)
	io.stderr:write(str, "\n")
	native.exit(1)
end

local environment = require "tundra.environment"
local nodegen = require "tundra.nodegen"
local decl = require "tundra.decl"
local path = require "tundra.path"
local cache = require "tundra.cache"

local SEP = native.host_platform == "windows" and "\\" or "/"
local default_env

GlobalEngine = nil

function match_build_id(id, default)
	assert(id)
	local i = id:gmatch("[^-]+")
	local platform_name, toolset, variant, subvariant = i() or default, i() or default, i() or default, i() or default
	return platform_name, toolset, variant, subvariant
end

local function run_build_script(text, fn)
	local script_globals, script_globals_mt = {}, {}
	script_globals_mt.__index = build_script_env
	setmetatable(script_globals, script_globals_mt)

	local chunk, error_msg = loadstring(text, fn)
	if not chunk then
		croak("%s", error_msg)
	end
	setfenv(chunk, script_globals)

	local function stack_dumper(err_obj)
		return debug.traceback(err_obj, 2)
	end

	local function args_stub()
		return chunk()
	end

	local success, result = xpcall(args_stub, stack_dumper)

	if not success then
		croak("%s", result)
	else
		return result
	end
end

local function apply_extension_module(id, default_package, ...)
	-- For non-qualified packages, use a default package
	if not id:find("%.") then
		id = default_package .. id
	end

	local pkg, err = require(id)

	if err then
		errorf("couldn't load extension module %s: %s", id, err)
	end

	pkg.apply(...)
end

function load_toolset(id, ...)
	apply_extension_module(id, "tundra.tools.", ...)
end

function load_syntax(id, decl, passes)
	apply_extension_module(id, "tundra.syntax.", decl, passes)
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

local function show_configs(configs, variants, subvariants, stream)
	stream = stream or io.stderr
	stream:write('Available configurations:\n')
	for _, config in pairs(configs) do
		if not config.Virtual then
			stream:write("    ", config.Name, "\n")
		end
	end

	stream:write('Available variants:\n')
	for variant, _ in pairs(variants) do
		stream:write("    ", variant, "\n")
	end

	stream:write('Available subvariants:\n')
	for subvariant, _ in pairs(subvariants) do
		stream:write("    ", subvariant, "\n")
	end
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
				build_variants[#build_variants + 1] = variants[name]
			elseif subvariants[name] then
				build_subvariants[#build_subvariants + 1] = name
			else
				local config, toolset, variant, subvariant = match_build_id(name)
				if config and variant then
					config = config .. "-".. toolset
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

					build_tuples[#build_tuples + 1] = { Config = configs[config], Variant = variants[variant], SubVariant = subvariant }
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
		for _, var in pairs(variants) do build_variants[#build_variants + 1] = var end
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
		if config.Virtual then
			croak("can't build configuration %s directly; it is a support configuration only", config.Name)
		end
		for _, variant in ipairs(build_variants) do
			for _, subvariant in ipairs(build_subvariants) do
				build_tuples[#build_tuples + 1] = { Config = config, Variant = variant, SubVariant = subvariant }
			end
		end
	end

	if #build_tuples == 0 then
		io.stderr:write("no build tuples available and no host-default configs defined -- choose one of\n")
		show_configs(configs, variants, subvariants)
		native.exit()
	end

	table.sort(remaining_targets)

	return remaining_targets, build_tuples
end

local function mk_defvariant(name) return { Name = name; Options = {} } end
local default_variants = { mk_defvariant "debug", mk_defvariant "production", mk_defvariant "release" }
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

local function setup_env(env, tuple, build_id)
	local config = tuple.Config
	local variant_name = tuple.Variant.Name

	if not build_id then
		build_id = config.Name .. "-" .. variant_name .. "-" .. tuple.SubVariant
		if Options.Verbose then
			printf("configuring for %s", build_id)
		end
	end

	local naked_platform, naked_toolset = match_build_id(build_id)

	env:set("CURRENT_PLATFORM", naked_platform) -- e.g. linux or macosx
	env:set("CURRENT_TOOLSET", naked_toolset) -- e.g. gcc or msvc
	env:set("CURRENT_VARIANT", tuple.Variant.Name) -- e.g. debug or release
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
			if type(val) == "table" then
				local list = nodegen.flatten_list(build_id, val)
				if Options.VeryVerbose then
					printf("Env Append %s => %s", util.tostring(val), util.tostring(list))
				end
				for _, subvalue in ipairs(list) do
					env:append(key, subvalue)
				end
			else
				env:append(key, val)
			end
		end
	end

	for env_tab in iter_inherits(config, "ReplaceEnv") do
		for key, val in util.pairs(env_tab) do
			if type(val) == "table" then
				local list = nodegen.flatten_list(build_id, val)
				if Options.VeryVerbose then
					printf("Env Replace %s => %s", util.tostring(val), util.tostring(list))
				end
				env:replace(key, subvalue)
			else
				env:replace(key, val)
			end
		end
	end

	-- Run post-setup functions. This typically sets up implicit make functions.
	env:run_setup_functions()

	return env
end

local function setup_envs(tuple, configs)
	local result = {}

	local top_env = setup_env(default_env:clone(), tuple)
	result["__default"] = top_env

	-- Use the same build id for all subconfigurations
	local build_id = top_env:get("BUILD_ID")

	local cfg = configs[tuple.Config.Name]
	for moniker, x in util.nil_pairs(cfg.SubConfigs) do
		if Options.VeryVerbose then
			printf("%s: setting up subconfiguration %s", tuple.Config.Name, x)
		end
		if result[x] then
			croak("duplicate subconfig name: %s", x)
		end
		local sub_tuple = { Config = configs[x], Variant = tuple.Variant, SubVariant = tuple.SubVariant }
		if not sub_tuple.Config then
			errorf("%s: no such config (in SubConfigs specification)", x)
		end
		local sub_env = setup_env(default_env:clone(), sub_tuple, build_id)
		result[moniker] = sub_env
	end
	return result
end

local function parse_units(build_tuples, args, passes)
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

	local raw_nodes, default_names, always_names = d:parse(args.Units or "units.lua")
	assert(#default_names > 0 or #always_names > 0, "no default unit name to build was set")
	return raw_nodes, default_names, always_names
end


local function create_build_engine(opts)
	local engine_opts = opts or {}

	return native.make_engine {
		-- per-session settings
		Verbosity = Options.Verbosity,
		ThreadCount = tonumber(Options.ThreadCount),
		ContinueOnError = Options.Continue and 1 or 0,
		DryRun = Options.DryRun and 1 or 0,
		DebugFlags = Options.DebugFlags,
		DebugSigning = Options.DebugSigning,

		-- per-config settings
		FileHashSize = engine_opts.FileHashSize,
		RelationHashSize = engine_opts.RelationHashSize,
		UseDigestSigning = engine_opts.UseDigestSigning,
	}
end

local cache_file = ".tundra-dagcache"
local cache_file_tmp = ".tundra-dagcache.tmp"

local function get_cached_dag(build_tuples, args, named_targets)
	local f = io.open(cache_file, "r")
	if not f then
		return nil
	end
	local data = f:read("*all")
	f:close()

	local chunk = assert(loadstring(data, cache_file))
	local env = setmetatable({}, { __index = _G })
	setfenv(chunk, env)
	chunk()

	local tuples_matched = 0
	for _, tuple in ipairs(build_tuples) do
		for _, data in ipairs(env.Tuples) do
			if data[1] == tuple.Config.Name and data[2] == tuple.Variant.Name and data[3] == tuple.SubVariant then
				tuples_matched = tuples_matched + 1
			end
		end
	end

	if tuples_matched ~= #build_tuples or tuples_matched ~= #env.Tuples then
		if Options.Verbose then
			print("discarding cached DAG due to build tuple mismatch")
		end
		return nil
	end

	local old_named_targets = env.NamedTargets
	if not old_named_targets then
		if Options.Verbose then
			print("discarding cached DAG due to missing NamedTargets list")
		end
		return nil
	end

	local function check_named()
		for _, name in ipairs(named_targets) do
			if not old_named_targets[name] then return nil end
		end

		local named_target_lookup = util.make_lookup_table(named_targets)
		for k,v in pairs(old_named_targets) do
			if not named_target_lookup[k] then return nil end
		end
		return true
	end

	if not check_named() then
		if Options.Verbose then
			print("discarding cached DAG due to different NamedTargets list")
		end
		return nil
	end

	for file, old_digest in pairs(env.Files) do
		local new_digest = get_file_digest(file)
		if new_digest ~= old_digest then
			if Options.Verbose then
				printf("discarding cached DAG as file %s has changed", file)
			end
			return nil
		end
	end

	for dir, digest in pairs(env.DirWalks) do
		-- get a non-recursive file list
		local files = native.walk_path(dir, function() return false end)
		util.map_in_place(files, function(fn) local _, file = path.split(fn); return file end)
		if cache.checksum_file_list(files) ~= digest then
			if Options.Verbose then
				printf("discarding cached DAG as contents of dir %s has changed", dir)
			end
			return nil
		end
	end

	if Options.Verbose then
		print("using cached DAG")
	end
	return env.CreateDag()
end

local function generate_dag(build_tuples, args, passes, configs, named_targets)
	local cache_flag = args.EngineOptions and args.EngineOptions.UseDagCaching
	if cache_flag then
		local cached_dag = get_cached_dag(build_tuples, args, named_targets)
		if cached_dag then
			return cached_dag
		end
	end

	-- This is a regular build. Assume these generator sets are always
	-- needed for now. Could possible make an option for which generator
	-- sets to load in the future.
	nodegen.add_generator_set("nodegen", "native")
	nodegen.add_generator_set("nodegen", "dotnet")

	local everything = {}

	if cache_flag then
		cache.open_cache(cache_file_tmp)
	end

	local raw_nodes, default_names, always_names = parse_units(build_tuples, args, passes)

	-- Let the nodegen code generate DAG nodes for all active
	-- configurations/variants.
	for _, tuple in pairs(build_tuples) do
		local envs = setup_envs(tuple, configs)
		everything[#everything + 1] = nodegen.generate {
			Engine = GlobalEngine,
			Envs = envs,
			Config = tuple.Config,
			Variant = tuple.Variant,
			Declarations = raw_nodes,
			DefaultNames = default_names,
			AlwaysNames = always_names,
			Passes = passes,
			NamedTargets = named_targets,
		}
	end

	local all = default_env:make_node {
		Label = "toplevel",
		Dependencies = everything,
	}

	if cache_flag then
		cache.commit_cache(build_tuples, accessed_lua_files, cache_file, named_targets)
	end

	return all
end

local _config_class = {}

-- Table constructor to make tundra.lua syntax a bit nicer in the Configs array
function build_script_env.Config(args)
	local name = args.Name
	if not name then
		error("no `Name' specified for configuration")
	end
	if not name:match("^%w+-%w+$") then
		errorf("configuration name %s doesn't follow <platform>-<toolset> pattern", name)
	end

	if args.SubConfigs then
		if not args.DefaultSubConfig then
			errorf("configuration %s has `SubConfigs' but no `DefaultSubConfig'", name)
		end
	end

	return setmetatable(args, _config_class)
end

function build_script_env.Build(args)
	if type(args.Configs) ~= "table" or #args.Configs == 0 then
		croak("Need at least one config; got %s" .. util.tostring(args.Configs) )
	end

	local configs, variants, subvariants = {}, {}, {}

	-- Legacy support: run "Config" constructor automatically on naked tables
	-- passed in Configs array.
	for idx = 1, #args.Configs do
		local cfg = args.Configs[idx]
		if getmetatable(cfg) ~= _config_class then
			cfg = build_script_env.Config(cfg)
			args.Configs[idx] = cfg
		end
		configs[cfg.Name] = cfg
	end

	for _, dir in util.nil_ipairs(args.ScriptDirs) do
		-- Make sure dir is sane and ends with a slash
		dir = dir:gsub("[/\\]", SEP):gsub("[/\\]$", "")
		local expr = dir .. SEP .. "?.lua"

		-- Add user toolset dir first so they can override builtin scripts.
		package.path = expr .. ";" .. package.path

		if Options.VeryVerbose then
			printf("package.path is now \"%s\"", package.path)
		end
	end

	if args.Variants then
		for i, x in ipairs(args.Variants) do
			if type(x) == "string" then
				args.Variants[i] = mk_defvariant(x)
			else
				assert(x.Name)
				if not x.Options then
					x.Options = {}
				end
			end
		end
	end

	local variant_array = args.Variants or default_variants
	for _, variant in ipairs(variant_array) do variants[variant.Name] = variant end

	local subvariant_array = args.SubVariants or default_subvariants
	for _, subvariant in ipairs(subvariant_array) do subvariants[subvariant] = true end

	local default_variant = variant_array[1]
	if args.DefaultVariant then
		for _, x in ipairs(variant_array) do
			if x.Name == args.DefaultVariant then
				default_variant = x
			end
		end
	end

	local default_subvariant = args.DefaultSubVariant or subvariant_array[1]
	local named_targets, build_tuples = analyze_targets(Targets, configs, variants, subvariants, default_variant, default_subvariant)
	local passes = args.Passes or { { Name = "Default", BuildOrder = 1 } }

	if Options.ShowConfigs then
		show_configs(configs, variants, subvariants, io.stdout)
		native.exit(0)
	end

	if not Options.IdeGeneration then
		GlobalEngine = create_build_engine(args.EngineOptions)

		local dag = generate_dag(build_tuples, args, passes, configs, named_targets)

		if Options.Clean then
			GlobalEngine:build(dag, "clean")
		else
			GlobalEngine:build(dag)
		end
	else
		-- We are generating IDE integration files. Load the specified
		-- integration module rather than DAG builders.
		nodegen.add_generator_set("ide", Options.IdeGeneration)

		-- Parse the units file
		local raw_nodes, default_names, always_names = parse_units(build_tuples, args, passes)

		-- Pass the build tuples directly to the generator and let it write
		-- files.
		local env = default_env:clone()
		nodegen.generate_ide_files(build_tuples, default_names, raw_nodes, env)
	end
end

function main(cmdline_args)
	TundraRootDir = assert(cmdline_args[1])

	-- Parse the command line options.
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
		{ Name="IdeGeneration", Short="g", Long="ide-gen", Doc="Generate IDE integration files for the specified IDE", HasValue=true },
		{ Name="AllConfigs", Short="a", Long="all-configs", Doc="Build all configurations at once (useful in IDE mode)" },
		{ Name="Cwd", Short="C", Long="set-cwd", Doc="Set the current directory before building", HasValue=true },
		{ Name="ShowConfigs", Long="show-configs", Doc="Show all available configurations" },
		{ Name="DebugQueue", Long="debug-queue", Doc="Show build queue debug information" },
		{ Name="DebugNodes", Long="debug-nodes", Doc="Show DAG node debug information" },
		{ Name="DebugAncestors", Long="debug-ancestors", Doc="Show ancestor debug information" },
		{ Name="DebugStats", Long="debug-stats", Doc="Show statistics on the build session" },
		{ Name="DebugReason", Long="debug-reason", Doc="Show build reasons" },
		{ Name="DebugScan", Long="debug-scan", Doc="Show dependency scanner debug information" },
		{ Name="DebugSigning", Long="debug-sign", Doc="Dump detailed signing log to `tundra-signdebug.txt'" },
		{ Name="Debugger", Long="lua-debugger", Doc="Run with Lua debugger enabled (use 'exit' to continue)" },
		{ Name="SelfTest", Long="self-test", Doc="Run a test of Tundra's internals" },
		{ Name="Profile", Long="lua-profile", Doc="Enable the Lua profiler" },
	}
	Options, Targets, message = util.parse_cmdline(cmdline_args, option_blueprints)
	if message then
		io.write(message, "\n")
		io.write("Try `tundra -h' to display an option summary\n")
		return 1
	end

	if Options.Profile then
		native.install_profiler()
	end

	if Options.Debugger then
		require "tundra.debugger"
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
		io.write("Tundra Build Processor v0.95 beta\n")
		io.write("Copyright (C) 2010 Andreas Fredriksson\n\n")
		io.write("This program comes with ABSOLUTELY NO WARRANTY.\n")
		io.write("This is free software, and you are welcome to redistribute it\n")
		io.write("under certain conditions; see the GNU GPL license for details.\n")
	end

	if Options.Help then
		io.write("\nCommand-line options:\n")
		for _, bp in ipairs(option_blueprints) do
			local h = string.format("  %- 3s %s",
			bp.Short and "-" .. bp.Short or "",
			bp.Long and "--" .. bp.Long or "")

			if bp.HasValue then
				h = h .. "=<value>"
			end

			if h:len() < 30 then
				h = h .. string.rep(" ", 30 - h:len())
			end

			io.write(h, bp.Doc or "", "\n")
		end
	end

	if Options.Version or Options.Help then
		return 0
	end

	if Options.SelfTest then
		require "tundra.selftest"
		native.exit(0)
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

	default_env = environment.create()
	default_env:set_many {
		["OBJECTROOT"] = "tundra-output",
		["SEP"] = SEP,
	}

	do
		local mod_name = "tundra.host." .. native.host_platform
		local mod = require(mod_name)
		if Options.Verbose then
			print("applying host settings from " .. mod_name)
		end
		mod.apply_host(default_env)
	end

	while true do
		local working_dir = native.get_cwd()
		if working_dir:sub(-1) ~= SEP then
			working_dir = working_dir .. SEP
		end
		local f, err = io.open('tundra.lua', 'r')
		if f then
			if Options.VeryVerbose then
				printf("found tundra.lua in %s", working_dir)
			end
			local data = f:read("*all")
			f:close()
			run_build_script(data, working_dir .. "tundra.lua")
			break
		else
			local parent_dir = working_dir:gsub("[^/\\]+[/\\]$", "")
			if working_dir == parent_dir or parent_dir:len() == 0 then
				croak("couldn't find tundra.lua here or anywhere up to \"%s\"", parent_dir)
			end
			if Options.VeryVerbose then
				printf("no tundra.lua in %s; trying %s", working_dir, parent_dir)
			end
			native.set_cwd(parent_dir)
		end
	end

	if Options.Profile then
		native.report_profiler("tundra.prof")
	end
end

