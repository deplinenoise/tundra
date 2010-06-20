module(..., package.seeall)

local util = require("tundra.util")
local path = require("tundra.path")
local native = require("tundra.native")

local default_pass = { Name = "Default", BuildOrder = 100000 }

function create_node(env_, data_)
	local function normalize_paths(paths)
		return util.mapnil(paths, function (x)
			return path.normalize(env_:interpolate(x))
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
		params.action = env_:interpolate(data_.Action, expand_env)
	else
		assert(not params.outputs, "can't have output files without an action")
		params.action = ""
	end

	params.annotation = env_:interpolate(data_.Label or "?", expand_env)

	return native_engine:make_node(params)
end

