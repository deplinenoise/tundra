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

local depgraph = require "tundra.depgraph"
local native = require "tundra.native"

local cache_file_tmp = nil
local cache_stream = nil
local cache_scanners = {}
local cache_data = {}
local cache_node_to_data = {}

function open_cache(tmp)
	cache_file_tmp = tmp
	cache_stream = assert(io.open(tmp, "w"))
	cache_stream:write("-- Tundra DAG cache file, don't edit\n\n")

	GlobalEngine:set_scanner_cache_hook(function(spec, object)
		cache_scanners[object] = spec
	end)

	GlobalEngine:set_node_cache_hook(function(spec, object)
		local index = #cache_data + 1
		cache_data[index] = spec
		cache_node_to_data[object] = index
	end)
end

function commit_cache(tuples, lua_files, cache_file)
	cache_stream:write("Tuples = {\n")
	for _, tuple in ipairs(tuples) do
		local cfg = tuple.Config.Name
		local variant = tuple.Variant.Name
		local subv = tuple.SubVariant
		local str = string.format("\t{ '%s', '%s', '%s' },\n", cfg, variant, subv)
		cache_stream:write(str)
	end
	cache_stream:write("}\n")

	cache_stream:write("Files = {\n")
	for name, digest in pairs(lua_files) do 
		cache_stream:write("\t'", name, "' = '", digest, "',\n")
	end
	cache_stream:write("}\n")

	local function dump_file_array(name, data)
		if data and #data > 0 then
			cache_stream:write("\t", name, " = {\n")
			for _, i in ipairs(data) do
				cache_stream:write(string.format("\t\t%q,\n", i))
			end
			cache_stream:write("\t},\n")
		end
	end

	local node_written = {}
	local env_block_index = 0
	local env_blocks = {}
	local scanner_index = 0
	local scanner_to_idx = {}

	local function emit_node(idx)
		if node_written[idx] then
			return
		end

		node_written[idx] = #node_written + 1
		local node = cache_data[idx]

		if node.deps then
			for _, dep in ipairs(node.deps) do
				emit_node(cache_node_to_data[dep])
			end
		end

		local my_scanner_idx
		if node.scanner then
			my_scanner_idx = scanner_to_idx[node.scanner]
			if not my_scanner_idx then
				local spec = assert(cache_scanners[node.scanner])
				scanner_index = scanner_index + 1
				my_scanner_idx = scanner_index
				scanner_to_idx[node.scanner] = my_scanner_idx
				cache_stream:write(string.format("Scanners[%d] = GlobalEngine:make_cpp_scanner {\n", my_scanner_idx))
				for _, path in ipairs(spec) do
					cache_stream:write(string.format("\t%q,\n", path))
				end
				cache_stream:write("}\n")
			end
		end

		if node.env and not env_blocks[node.env] then
			env_block_index = env_block_index + 1
			env_blocks[node.env] = env_block_index
			cache_stream:write(string.format("Envs[%d] = {\n)", env_block_index))
			for k, v in pairs(node.env) do
				cache_stream:write(string.format("\t%q = %q,\n", k, v))
			end
			cache_stream:write("}\n\n")
		end

		cache_stream:write(string.format("Nodes[%d] = GlobalEngine:make_node {\n", idx))
		cache_stream:write(string.format("\taction = %q,\n", node.action))
		cache_stream:write(string.format("\tannotation = %q,\n", node.annotation))
		cache_stream:write(string.format("\tsalt = %q,\n", node.salt))

		dump_file_array("inputs", node.inputs)
		dump_file_array("outputs", node.outputs)
		dump_file_array("aux_outputs", node.aux_outputs)

		if node.env then
			cache_stream:write(string.format("\tenv = Envs[%d],\n", env_blocks[node.env]))
		end

		if my_scanner_idx then
			cache_stream:write(string.format("\tscanner = Scanners[%d],\n", my_scanner_idx))
		end

		if node.deps and #node.deps > 0 then
			cache_stream:write("\tdeps = { ")
			for _, dep in ipairs(node.deps) do
				local idx = assert(cache_node_to_data[dep])
				local serialized_index = assert(node_written[idx])
				cache_stream:write(string.format("Nodes[%d], ", serialized_index))
			end
			cache_stream:write("},\n")
		end
		cache_stream:write("}\n\n")
	end

	cache_stream:write("Scanners = {}\n")
	cache_stream:write("Nodes = {}\n")
	cache_stream:write("Envs = {}\n\n")
	for idx, node in ipairs(cache_data) do
		emit_node(idx)
	end

	cache_stream:close()
	cache_stream = nil
	native.delete_file(cache_file)
	assert(native.rename_file(cache_file_tmp, cache_file))
end

