module(..., package.seeall)

local util = require("tundra.util")
local path = require("tundra.path")
local native = require("tundra.native")

local default_pass = { Name = "Default", BuildOrder = 100000 }

function CreateNode(env_, data_)
	local function normalize_paths(paths)
		return util.mapnil(paths, function (x)
			return path.NormalizePath(env_:Interpolate(x))
		end)
	end

	local params = {
		pass = data_.Pass or default_pass,
		scanner = data_.Scanner,
		deps = data_.Dependencies,
		inputs = normalize_paths(data_.InputFiles),
		outputs = normalize_paths(data_.OutputFiles),
	}

	local expand_env = {
		['<'] = params.inputs,
		['@'] = params.outputs
	}

	if data_.Action then
		params.action = env_:Interpolate(data_.Action, expand_env)
	else
		assert(not params.outputs, "can't have output files without an action")
		params.action = ""
	end

	params.annotation = env_:Interpolate(data_.Label or "?", expand_env)

	return native_engine:make_node(params)
end

function SpliceOutputsSingle(node, exts)
	local result = {}
	local o = node:GetOutputFiles()
	if o then
		for _, output in ipairs(o) do
			table.insert(result, output)
		end
	end

	if exts then
		util.FilterInPlace(result, function (fn)
			local ext = path.GetExtension(fn)
			for idx = 1, #exts do
				if ext == exts[idx] then
					return true
				end
			end
			return false
		end)
	end

	return result
end

function SpliceOutputs(nodes, exts)
	local result = {}
	for _, node in ipairs(nodes) do
		local o = node:GetOutputFiles()
		if o then
			for _, output in ipairs(o) do
				table.insert(result, output)
			end
		end
	end

	if exts then
		util.FilterInPlace(result, function (fn)
			local ext = path.GetExtension(fn)
			for idx = 1, #exts do
				if ext == exts[idx] then
					return true
				end
			end
			return false
		end)
	end
	return result
end

