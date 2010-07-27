module(..., package.seeall)

local util = require "tundra.util"
local path = require "tundra.path"
local native = require "tundra.native"
local depgraph = require "tundra.depgraph"

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
	local env = assert(args.Env)
	local raw_nodes = assert(args.Declarations)
	local default_names = assert(args.DefaultNames)

	local state = make_new_state {
		base_env = env,
		config = assert(args.Config),
		variant = assert(args.Variant),
		passes = assert(args.Passes),
	}

	create_unit_map(state, raw_nodes)

	local nodes_to_build = util.map(default_names, function (name) return state:get_node_of(name) end)

	local result = env:make_node {
		Label = "all-" .. env:get("BUILD_ID"),
		Dependencies = nodes_to_build,
	}

	return result
end

function _generator:get_node_of(name)
	assert(name)
	local n = self.unit_nodes[name]
	if not n then
		self.unit_nodes[name] = "!"
		n = self:eval_unit(assert(self.units[name]))
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

function _generator:resolve_sources(env, items, accum, base_dir)
	base_dir = base_dir or ""
	local header_exts = {}
	for _, ext in ipairs(env:get_list("HEADERS_EXTS")) do
		header_exts[ext] = true
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
				self:resolve_sources(env, item, accum, base_dir)
			end
		else
			assert(type_name == "string")
			local ext = path.get_extension(item)
			if not header_exts[ext] then
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

	local result = {}
	for _, src in ipairs(files) do
		transform(result, src)
	end

	return result, deps
end

function _generator:get_target(decl, suffix)
	local target = decl.Target
	if not target then
		target = "$(OBJECTDIR)/" .. decl.Name .. suffix
	end
	return target
end

function _generator:eval_unit(unit)
	local unit_env = self.base_env:clone()
	local decl = unit.Decl
	local unit_type = unit.Type
	local eval_fn = self.evaluators[unit_type]

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
	local chunk = assert(loadfile(fn))
	chunk(_generator)
end

local function config_filter_match(pattern, build_id)
	if pattern then
		local p = '^' .. pattern:gsub('-', '%%-'):gsub('%*', '%%w-') .. '$'
		local res = string.match(build_id, p)
		--printf("matching %s with %s (from %s) => %s", p, build_id, pattern, util.tostring(res))
		return res
	else
		return true
	end
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
			if config_filter_match(data.Config, build_id) then
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
	local state = make_new_state { base_env = env }
	assert(state.base_env)
	create_unit_map(state, raw_nodes)
	state:generate_files(config_tuples, raw_nodes, env)
end

