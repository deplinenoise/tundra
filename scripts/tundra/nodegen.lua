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

local util = require "tundra.util"
local path = require "tundra.path"
local native = require "tundra.native"

local ide_backend = nil

_generator = {
	evaluators = {},
}
_generator.__index = _generator

local function make_new_state(s)
	s = s or {}
	s.units = {}
	s.unit_nodes = {}
	return setmetatable(s, _generator)
end

local function create_unit_map(state, raw_nodes)
	-- Build name=>decl mapping
	for _, unit in ipairs(raw_nodes) do
		assert(unit.Decl)
		local name = assert(unit.Decl.Name)
		if state.units[name] then
			errorf("duplicate unit name: %s", name)
		end
		state.units[name] = unit
	end
end

function generate(args)
	local envs = assert(args.Envs)
	local raw_nodes = assert(args.Declarations)
	local default_names = assert(args.DefaultNames)

	local state = make_new_state {
		base_envs = envs,
		root_env = envs["__default"], -- the outmost config's env in a cross-compilation scenario
		config = assert(args.Config),
		variant = assert(args.Variant),
		passes = assert(args.Passes),
	}

	local subconfigs = state.config.SubConfigs

	-- Pick a default environment which is used for
	-- 1. Nodes without a SubConfig declaration
	-- 2. Nodes with a missing SubConfig declaration
	-- 3. All nodes if there are no SubConfigs set for the current config
	if subconfigs then
		state.default_subconfig = assert(state.config.DefaultSubConfig)
		state.default_env = envs[state.default_subconfig]
	else
		state.default_env = assert(envs["__default"])
	end

	create_unit_map(state, raw_nodes)

	local nodes_to_build = util.map(default_names, function (name) return state:get_node_of(name) end)

	local result = state.root_env:make_node {
		Label = "all-" .. state.root_env:get("BUILD_ID"),
		Dependencies = nodes_to_build,
	}

	return result
end

function _generator:get_node_of(name)
	assert(name)
	local n = self.unit_nodes[name]
	if not n then
		self.unit_nodes[name] = "!"
		local u = self.units[name]
		if not u then
			errorf("couldn't find unit %s", name)
		end
		n = self:eval_unit(u)
		self.unit_nodes[name] = n
	else
		assert(n ~= "!")
	end
	return n
end

function _generator:resolve_pass(name)
	return self.passes[name]
end

function _generator:resolve_deps(build_id, deps)
	if not deps then
		return nil
	end

	deps = flatten_list(build_id, deps)

	local result = {}
	for i, dep in ipairs(deps) do
		result[i] = self:get_node_of(dep)
	end
	return result
end

function _generator:resolve_sources(env, items, accum, base_dir, include_headers)
	base_dir = base_dir or ""
	local header_exts
	if not include_headers then
		header_exts = util.make_lookup_table(env:get_list("HEADERS_EXTS"))
	end

	for _, item in util.nil_ipairs(items) do
		local type_name = type(item)

		while type_name == "function" do
			item = item(env)
			type_name = type(item)
		end

		if type_name == "userdata" then
			accum[#accum + 1] = item
		elseif type_name == "table" then
			if getmetatable(item) then
				accum[#accum + 1] = item
			else
				self:resolve_sources(env, item, accum, base_dir, include_headers)
			end
		else
			assert(type_name == "string")
			local ext = path.get_extension(item)
			if not header_exts or not header_exts[ext] then
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
-- transformer (optional) - transformer function to make nodes from plain filse
--
function _generator:analyze_sources(list, suffixes, transformer)
	if not list then
		return nil
	end

	list = util.flatten(list)
	local deps = {}

	local function transform(output, fn)
		if type(fn) ~= "string" then
			error(util.tostring(fn) .. " is not a string", 2)
		end
		if transformer then
			local t = transformer(fn)
			if t then
				deps[#deps + 1] = t
				t:insert_output_files(output, suffixes)
			else
				output[#output + 1] = fn
			end
		else
			output[#output + 1] = fn
		end
	end

	local files = {}
	for _, src in ipairs(list) do
		if native.is_node(src) then
			deps[#deps + 1] = src
			src:insert_output_files(files, suffixes)
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
			return result, deps
		end
	end
end

function _generator:get_target(decl, suffix)
	local target = decl.Target
	if not target then
		target = "$(OBJECTDIR)/" .. decl.Name .. suffix
	end
	return target
end

local pattern_cache = {}
local function get_cached_pattern(p)
	local v = pattern_cache[p]
	if not v then
		local comp = '%w+'
		local sub_pattern = p:gsub('*', '%%w+')
		local platform, tool, variant, subvariant = match_build_id(sub_pattern, comp)
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

function _generator:eval_unit(unit)
	-- Select an environment for this unit based on its SubConfig tag
	-- to support cross compilation.
	local env
	local subconfig = unit.Decl.SubConfig
	if subconfig then
		env = self.base_envs[subconfig]
		if Options.VeryVerbose then
			if env then
				printf("%s: using subconfig %s (%s)", unit.Decl.Name, subconfig, env:get('BUILD_ID'))
			else
				printf("%s: no subconfig %s found; using default env", unit.Decl.Name, subconfig)
			end
		end
	end

	if not env then
		env = self.default_env
	end

	local unit_env = env:clone()

	local decl = unit.Decl
	local unit_type = unit.Type
	local eval_fn = self.evaluators[unit_type]
	local build_id = unit_env:get("BUILD_ID", "")

	if build_id:len() > 0 and not config_matches(decl.Config, unit_env:get("BUILD_ID")) then
		return unit_env:make_node { Label = "Dummy node for " .. decl.Name }
	end

	if not eval_fn then
		error(string.format("%s: unsupported unit type", unit_type))
	end

	return eval_fn(self, unit_env, decl)
end

function add_evaluator(name, fn)
	_generator.evaluators[name] = fn
end

function add_generator_set(type_name, id)
	local fn = TundraRootDir .. "/scripts/tundra/" .. type_name .. "/" .. id .. ".lua"
	local chunk, err = loadfile(fn)
	if not chunk then
		croak("couldn't load %s file %s (%s): %s", type_name, id, fn, err)
	end
	chunk(_generator)
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
	--print(util.tostring(result) .." => " .. util.tostring(result))
	return result
end

function generate_ide_files(config_tuples, default_names, raw_nodes, env)
	local state = make_new_state { default_env = env }
	assert(state.default_env)
	create_unit_map(state, raw_nodes)
	local backend_fn = assert(ide_backend)
	backend_fn(state, config_tuples, raw_nodes, env)
end

function set_ide_backend(backend_fn)
	ide_backend = backend_fn
end

