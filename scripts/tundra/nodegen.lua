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

local boot = require "tundra.boot"
local util = require "tundra.util"
local path = require "tundra.path"
local native = require "tundra.native"

local ide_backend = nil

local current = nil

local _nodegen = { }
_nodegen.__index = _nodegen

local function syntax_error(msg, ...)
	error { Class = 'syntax error', Message = string.format(msg, ...) }
end

local function validate_boolean(name, value)
	if type(value) == "boolean" then
		return value
	end
	syntax_error("%s: expected boolean value, got %q", name, type(value))
end

local function validate_string(name, value)
	if type(value) == "string" then
		return value
	end
	syntax_error("%s: expected string value, got %q", name, type(value))
end

local function validate_pass(name, value)
	if type(value) == "string" then
        return value
    else
        syntax_error("%s: expected pass name, got %q", name, type(value))
	end
end

local function validate_table(name, value)
	-- A single string can be converted into a table value very easily
	local t = type(value)
	if t == "table" then
		return value
	elseif t == "string" then
		return { value }
	else
		syntax_error("%s: expected table value, got %q", name, t)
	end
end

local function validate_config(name, value)
	if type(value) == "table" or type(value) == "string" then
		return value
	end
	syntax_error("%s: expected config, got %q", name, type(value))
end

local validators = {
	["string"] = validate_string,
	["pass"] = validate_pass,
	["table"] = validate_table,
	["filter_table"] = validate_table,
	["source_list"] = validate_table,
	["boolean"] = validate_boolean,
	["config"] = validate_config,
}

function _nodegen:validate()
	local decl = self.Decl
	for name, detail in pairs(assert(self.Blueprint)) do
		local val = decl[name]
		if not val then
			if detail.Required then
				syntax_error("%s: missing argument: '%s'", self.Keyword, name)
			end
			-- ok, optional value
		else
			local validator = validators[detail.Type]
			decl[name] = validator(name, val)
		end
	end
	for name, detail in pairs(decl) do
		if not self.Blueprint[name] then
			syntax_error("%s: unsupported argument: '%s'", self.Keyword, name)
		end
	end
end

function _nodegen:customize_env(env, raw_data)
	-- available for subclasses
end

function _nodegen:configure_env(env, deps)
	local build_id = env:get('BUILD_ID')
	local propagate_blocks = {}
	local decl = self.Decl

	for _, dep_obj in util.nil_ipairs(deps) do
		local data = dep_obj.Decl.Propagate
		if data then
			propagate_blocks[#propagate_blocks + 1] = data
		end
	end

	local function push_bindings(env_key, data)
		if data then
			for _, item in util.nil_ipairs(flatten_list(build_id, data)) do
				env:append(env_key, item)
			end
		end
	end

	local function replace_bindings(env_key, data)
		if data then
			local first = true
			for _, item in util.nil_ipairs(flatten_list(build_id, data)) do
				if first then
					env:replace(env_key, item)
					first = false
				else
					env:append(env_key, item)
				end
			end
		end
	end

	-- Push Libs, Defines and so in into the environment of this unit.
	-- These are named for convenience but are aliases for syntax niceness.
	for decl_key, env_key in util.nil_pairs(self.DeclToEnvMappings) do
		-- First pick settings from our own unit.
		push_bindings(env_key, decl[decl_key])

		for _, data in ipairs(propagate_blocks) do
			push_bindings(env_key, data[decl_key])
		end
	end

	-- Push Env blocks as is
	for k, v in util.nil_pairs(decl.Env) do
		push_bindings(k, v)
	end

	for k, v in util.nil_pairs(decl.ReplaceEnv) do
		replace_bindings(k, v)
	end

	for _, block in util.nil_ipairs(propagate_blocks) do
		for k, v in util.nil_pairs(block.Env) do
			push_bindings(k, v)
		end

		for k, v in util.nil_pairs(block.ReplaceEnv) do
			replace_bindings(k, v)
		end
	end
end

local function resolve_sources(env, items, accum, base_dir)
	local ignored_exts = util.make_lookup_table(env:get_list("IGNORED_AUTOEXTS", {}))
	base_dir = base_dir or ""
	for _, item in util.nil_ipairs(items) do
		local type_name = type(item)

		assert(type_name ~= "function")

		--while type_name == "function" do
		--	item = item(env)
		--	type_name = type(item)
		--end

		if type_name == "userdata" then
			accum[#accum + 1] = item
		elseif type_name == "table" then
			if native.is_node(item) then
				accum[#accum + 1] = item
			elseif getmetatable(item) then
				accum[#accum + 1] = item:get_dag(env)
			else
				resolve_sources(env, item, accum, base_dir)
			end
		else
			assert(type_name == "string")
			local ext = path.get_extension(item)
			if not ignored_exts[ext] then
				accum[#accum + 1] = base_dir .. item
			end
		end
	end
	return accum
end

-- Analyze source list, returning list of input files and list of dependencies.
--
-- This is so you can pass a mix of actions producing files and regular
-- filenames as inputs to the next step in the chain and the output files of
-- such nodes will be used automatically.
--
-- list - list of source files and nodes that produce source files
-- suffixes - acceptable source suffixes to pick up from nodes in source list
local function analyze_sources(env, pass, list, suffixes)
	if not list then
		return nil
	end

	list = util.flatten(list)
	local deps = {}

	local function implicit_make(source_file)
		local t = type(source_file)
		if t == "table" then
			return source_file
		end
		assert(t == "string")

		local make = env:get_implicit_make_fn(source_file)
		if make then
			return make(env, pass, source_file)
		else
			return nil
		end
	end

	local function transform(output, fn)
		if type(fn) ~= "string" then
			error(util.tostring(fn) .. " is not a string", 2)
		end

		local t = implicit_make(fn)
		if t then
			deps[#deps + 1] = t
			t:insert_output_files(output, suffixes)
		else
			output[#output + 1] = fn
		end
	end

	local files = {}
	for _, src in ipairs(list) do
		if native.is_node(src) then
			deps[#deps + 1] = src
			src:insert_output_files(files, suffixes)
		elseif type(src) == "table" then
			error("non-DAG node in source list at this point")
		else
			files[#files + 1] = src
		end
	end

	while true do
		local result = {}
		local old_dep_count = #deps
		for _, src in ipairs(files) do
			transform(result, src)
		end
		files = result
		if #deps == old_dep_count then
			--print("scan", util.tostring(list), util.tostring(suffixes), util.tostring(result))
			return result, deps
		end
	end
end

local function x_identity(self, name, info, value, env, out_deps)
	return value
end

local function x_source_list(self, name, info, value, env, out_deps)
	local build_id = env:get('BUILD_ID')
	local source_files = flatten_list(build_id, value)
	local sources = resolve_sources(env, source_files, {}, self.Decl.SourceDir)
	local source_exts = env:get_list(info.ExtensionKey)
	local inputs, ideps = analyze_sources(env, resolve_pass(self.Decl.Pass), sources, source_exts)
	if ideps then
		util.append_table(out_deps, ideps)
	end
	return inputs
end

local function x_filter_table(self, name, info, value, env, out_deps)
	local build_id = env:get('BUILD_ID')
	return flatten_list(build_id, value)
end

local function find_named_node(name_or_dag)
	if type(name_or_dag) == "table" then
		return name_or_dag:get_dag(current.default_env)
	elseif type(name_or_dag) == "string" then
		local generator = current.units[name_or_dag]
		if not generator then
			errorf("unknown node specified: %q", tostring(name_or_dag))
		end
		return generator:get_dag(current.default_env)
	else
		errorf("illegal node specified: %q", tostring(name_or_dag))
	end
end

-- Special resolver for dependencies in a nested (config-filtered) list.
local function resolve_dependencies(decl, raw_deps, env)
  if not raw_deps then
    return {}
  end

	local build_id = env:get('BUILD_ID')
	local deps = flatten_list(build_id, raw_deps)
	return util.map_in_place(deps, function (i)
		if type(i) == "string" then
			n = current.units[i]
			if not n then
				errorf("%s: Unknown 'Depends' target %q", decl.Name, i)
			end
			return n
		elseif type(i) == "table" and getmetatable(i) and i.Decl then
			return i
		else
			errorf("bad 'Depends' value of type %q", type(i))
		end
	end)
end

local function x_pass(self, name, info, value, env, out_deps)
    return resolve_pass(value)
end

local decl_transformers = {
	-- the x_identity data types have already been checked at script time through validate_xxx
	["string"] = x_identity,
	["table"] = x_identity,
	["config"] = x_identity,
	["boolean"] = x_identity,
	["pass"] = x_pass,
	["source_list"] = x_source_list,
	["filter_table"] = x_filter_table,
}

-- Create input data for the generator's DAG creation function based on the
-- blueprint passed in when the generator was registered. This is done here
-- centrally rather than in all the different node generators to reduce code
-- duplication and keep the generators miminal. If you need to do something
-- special, you can override create_input_data() in your subclass.
function _nodegen:create_input_data(env)
	local decl = self.Decl
	local data = {}
	local deps = {}

	for name, detail in pairs(assert(self.Blueprint)) do
		local val = decl[name]
		if val then
			local xform = decl_transformers[detail.Type]
			data[name] = xform(self, name, detail, val, env, deps)
		end
	end

	return data, deps
end

function get_pass(self, name)
	if not name then
		return nil
	end

end

local pattern_cache = {}
local function get_cached_pattern(p)
	local v = pattern_cache[p]
	if not v then
		local comp = '[%w_]+'
		local sub_pattern = p:gsub('*', '[%%w_]+')
		local platform, tool, variant, subvariant = boot.match_build_id(sub_pattern, comp)
		v = string.format('^%s%%-%s%%-%s%%-%s$', platform, tool, variant, subvariant)
		pattern_cache[p] = v
	end
	return v
end

local function config_matches(pattern, build_id)
	local ptype = type(pattern)
	if ptype == "nil" then
		return true
	elseif ptype == "string" then
		local fpattern = get_cached_pattern(pattern)
		return build_id:match(fpattern)
	elseif ptype == "table" then
		for _, pattern_item in ipairs(pattern) do
			if config_matches(pattern_item, build_id) then
				return true
			end
		end
		return false
	else
		error("bad 'Config' pattern type: " .. ptype)
	end
end

local function make_unit_env(unit)
	-- Select an environment for this unit based on its SubConfig tag
	-- to support cross compilation.
	local env
	local subconfig = unit.Decl.SubConfig or current.default_subconfig
	if subconfig and current.base_envs then
		env = current.base_envs[subconfig]
		if tundra.boot.Options.VeryVerbose then
			if env then
				printf("%s: using subconfig %s (%s)", unit.Decl.Name, subconfig, env:get('BUILD_ID'))
			else
				if current.default_subconfig then
					errorf("%s: couldn't find a subconfig env", unit.Decl.Name)
				else
					printf("%s: no subconfig %s found; using default env", unit.Decl.Name, subconfig)
				end
			end
		end
	end

	if not env then
		env = current.default_env
	end

	return env:clone()
end

function _nodegen:get_dag(parent_env)
	local build_id = parent_env:get('BUILD_ID')
	local dag = self.DagCache[build_id]
	if not dag then
		if build_id:len() > 0 and not config_matches(self.Decl.Config, build_id) then
			-- Unit has been filtered out via Config attribute.
			dag = parent_env:make_node {
				Label = "Dummy node for " .. self.Decl.Name
			}
		else
			local unit_env = make_unit_env(self)

			if self.Decl.Name then
				unit_env:set('UNIT_PREFIX', '__' .. self.Decl.Name)
			end

      -- Before accessing the unit's dependencies, resolve them via filtering.
      local deps = resolve_dependencies(self.Decl, self.Decl.Depends, unit_env)

			self:configure_env(unit_env, deps)
			self:customize_env(unit_env, self.Decl, deps)

			local input_data, input_deps = self:create_input_data(unit_env, parent_env)
      -- Copy over dependencies which have been pre-resolved
      input_data.Depends = deps

			for _, dep in util.nil_ipairs(deps) do
				input_deps[#input_deps + 1] = dep:get_dag(parent_env)
			end
			dag = self:create_dag(unit_env, input_data, input_deps, parent_env)
			if not dag then
				error("create_dag didn't generate a result node")
			end
		end
		self.DagCache[build_id] = dag
	end
	return dag
end

local _generator = {
	Evaluators = {},
}
_generator.__index = _generator

local function new_generator(s)
	s = s or {}
	s.units = {}
	return setmetatable(s, _generator)
end

local function create_unit_map(state, raw_nodes)
	-- Build name=>decl mapping
	for _, unit in ipairs(raw_nodes) do
		assert(unit.Decl)
		local name = unit.Decl.Name
		if name and type(name) == "string" then
			if state.units[name] then
				errorf("duplicate unit name: %s", name)
			end
			state.units[name] = unit
		end
	end
end

function generate_dag(args)
	local envs = assert(args.Envs)
	local raw_nodes = assert(args.Declarations)

	local state = new_generator {
		base_envs = envs,
		root_env = envs["__default"], -- the outmost config's env in a cross-compilation scenario
		config = assert(args.Config),
		variant = assert(args.Variant),
		passes = assert(args.Passes),
	}

	current = state

	create_unit_map(state, raw_nodes)

	local subconfigs = state.config.SubConfigs

	-- Pick a default environment which is used for
	-- 1. Nodes without a SubConfig declaration
	-- 2. Nodes with a missing SubConfig declaration
	-- 3. All nodes if there are no SubConfigs set for the current config
	if subconfigs then
		state.default_subconfig = assert(state.config.DefaultSubConfig)
		state.default_env = assert(envs[state.default_subconfig], "unknown DefaultSubConfig specified")
	else
		state.default_env = assert(envs["__default"])
	end

	local nodes_to_build = {}

	local name_set = {}
	if #args.NamedTargets > 0 then
		for _, name in ipairs(args.NamedTargets) do
			name_set[name] = find_named_node(name)
		end
	else
		for _, name in ipairs(args.DefaultNames) do
			name_set[name] = find_named_node(name)
		end
	end

	for _, name in ipairs(args.AlwaysNames) do
		name_set[name] = find_named_node(name)
	end

	local nodes_to_build = {}
	for _, v in pairs(name_set) do
		nodes_to_build[#nodes_to_build + 1] = v
	end

	local result = state.root_env:make_node {
		Label = "all-" .. state.root_env:get("BUILD_ID"),
		Dependencies = nodes_to_build,
	}

	current = nil

	return result
end

function resolve_pass(name)
	assert(current)
	if name then
		local p = current.passes[name]
        if not p then
            syntax_error("%q is not a valid pass name", name)
        end
        return p
    else
        return nil
    end
end

function get_target(data, suffix, prefix)
	local target = data.Target
	if not target then
		assert(data.Name)
		target = "$(OBJECTDIR)/" .. (prefix or "") .. data.Name .. (suffix or "")
	end
	return target
end

function is_evaluator(name)
	if _generator.Evaluators[name] then return true else return false end
end

local common_blueprint = {
	Propagate = {
		Help = "Declarations to propagate to dependent units",
		Type = "filter_table",
	},
  Depends = {
    Help = "Dependencies for this node",
    Type = "table", -- handled specially
  },
	Env = {
		Help = "Data to append to the environment for the unit",
		Type = "filter_table",
	},
	ReplaceEnv = {
		Help = "Data to replace in the environment for the unit",
		Type = "filter_table",
	},
	Pass = {
		Help = "Specify build pass",
		Type = "pass",
	},
	SourceDir = {
		Help = "Specify base directory for source files",
		Type = "string",
	},
	Config = {
		Help = "Specify configuration this unit will build in",
		Type = "config",
	},
	SubConfig = {
		Help = "Specify sub-configuration this unit will build in",
		Type = "config",
	},
}

function create_eval_subclass(meta_tbl, base)
	base = base or _nodegen
	setmetatable(meta_tbl, base)
	meta_tbl.__index = meta_tbl
	return meta_tbl
end

function add_evaluator(name, meta_tbl, blueprint)
	assert(type(name) == "string")
	assert(type(meta_tbl) == "table")
	assert(type(blueprint) == "table")

	-- Set up this metatable as a subclass of _nodegen unless it is already
	-- configured.
	if not getmetatable(meta_tbl) then
		setmetatable(meta_tbl, _nodegen)
		meta_tbl.__index = meta_tbl
	end

	-- Install common blueprint items.
	for name, val in pairs(common_blueprint) do
		if not blueprint[name] then
			blueprint[name] = val
		end
	end

	-- Expand environment shortcuts into options.
	for decl_key, env_key in util.nil_pairs(meta_tbl.DeclToEnvMappings) do
		blueprint[decl_key] = {
			Type = "filter_table",
			Help = "Shortcut for environment key " .. env_key,
		}
	end

	for name, val in pairs(blueprint) do
		local type_ = assert(val.Type)
		if not validators[type_] then
			errorf("unsupported blueprint type %q", type_)
		end

        if val.Type == "source_list" and not val.ExtensionKey then
			errorf("%s: source_list must provide ExtensionKey", name)
        end
	end

	-- Record blueprint for use when validating user constructs.
	meta_tbl.Keyword = name
	meta_tbl.Blueprint = blueprint

	-- Store this evaluator under the keyword that will trigger it.
	_generator.Evaluators[name] = meta_tbl
end

-- Called when processing build scripts, keywords is something previously
-- registered as an evaluator here.
function evaluate(eval_keyword, data)
	local meta_tbl = assert(_generator.Evaluators[eval_keyword])
	local object = setmetatable({
		DagCache = {}, -- maps BUILD_ID -> dag node
		Decl = data
	}, meta_tbl)
	object.__index = object
	object:validate()
	return object
end

-- Given a list of strings or nested lists, flatten the structure to a single
-- list of strings while applying configuration filters. Configuration filters
-- match against the current build identifier like this:
--
-- { "a", "b", { "nixfile1", "nixfile2"; Config = "unix-*-*" }, "bar", { "debugfile"; Config = "*-*-debug" }, }
function flatten_list(build_id, list)
	if not list then return nil end

	-- Helper function to apply filtering recursively and append results to an
	-- accumulator table.
	local function iter(data, accum)
		local t = type(data)
		if t == "table" and not getmetatable(data) then
			if config_matches(data.Config, build_id) then
				for _, item in ipairs(data) do
					iter(item, accum)
				end
			end
		else
			accum[#accum + 1] = data
		end
	end

	local result = {}
	iter(list, result)
	return result
end

function generate_ide_files(config_tuples, default_names, raw_nodes, env)
	local state = new_generator { default_env = env }
	assert(state.default_env)
	create_unit_map(state, raw_nodes)
	local backend_fn = assert(ide_backend)
	backend_fn(state, config_tuples, raw_nodes, env, default_names)
end

function set_ide_backend(backend_fn)
	ide_backend = backend_fn
end

