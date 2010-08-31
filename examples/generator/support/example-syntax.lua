
module(..., package.seeall)

local environment = require "tundra.environment"

function apply(decl_parser, passes)
	-- Add a function ExampleGenerator that accepts the following args:
	-- Source: the source text file to read
	-- OutName: the base name of the generated C file
	decl_parser:add_source_generator("ExampleGenerator", function (args)
		local source = assert(args.Source, "no source file specified!")
		local outname = assert(args.OutName, "no output name specified!")
		local full_fn = "$(OBJECTDIR)/_generated/" .. outname
		return function (env)
			return env:make_node {
				Label = "ExampleGen $(@)",
				Action = "$(EXAMPLEGEN) $(<) $(@)",
				Pass = passes.CodeGeneration,
				InputFiles = { source },
				OutputFiles = { full_fn },
				ImplicitInputs = { "$(EXAMPLEGEN)" },
			}
		end
	end)
end
