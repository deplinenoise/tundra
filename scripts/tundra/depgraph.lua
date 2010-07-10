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

	-- these are the inputs that $(<) expand to
	local regular_inputs = normalize_paths(data_.InputFiles)

	-- these are other, auxillary input files that shouldn't appear on the command line
	-- useful to e.g. add an input dependency on a tool
	local implicit_inputs = normalize_paths(data_.ImplicitInputs)

	local params = {
		pass = data_.Pass or default_pass,
		scanner = data_.Scanner,
		deps = data_.Dependencies,
		inputs = util.merge_arrays_2(regular_inputs, implicit_inputs),
		outputs = normalize_paths(data_.OutputFiles),
	}

	local expand_env = {
		['<'] = normalize_paths(regular_inputs),
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

