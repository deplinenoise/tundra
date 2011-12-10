
module(..., package.seeall)

local nodegen = require "tundra.nodegen"

local _generator_mt = nodegen.create_eval_subclass { }

function _generator_mt:create_dag(env, data, deps)
		return env:make_node {
			Label = "MyGenerator $(@)",
			Action = "$(MYGENERATOR) $(@)",
			Pass = data.Pass,
			OutputFiles = { "$(OBJECTDIR)/_generated/" .. data.OutName },
			ImplicitInputs = { "$(MYGENERATOR)" },
			Dependencies = deps,
		}
end

local blueprint = {
	OutName = { Type = "string", Required = true },
}

nodegen.add_evaluator("MyGenerator", _generator_mt, blueprint)
