
local decl_parser, passes = ...
local environment = require "tundra.environment"

decl_parser:add_source_generator("MyGenerator", function (args)
	local outname = assert(args.OutName, "no output name specified!")
	local full_fn = "$(OBJECTDIR)/_generated/" .. outname
	return function (env)
		return env:make_node {
			Label = "MyGenerator $(@)",
			Action = "$(MYGENERATOR) $(@)",
			Pass = passes.CodeGeneration,
			OutputFiles = { full_fn },
			ImplicitInputs = { "$(MYGENERATOR)" },
		}
	end
end)

