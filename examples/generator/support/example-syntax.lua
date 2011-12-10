
module(..., package.seeall)

local nodegen = require "tundra.nodegen"

local _examplegen = {}

-- The function that will be called upon to generate a DAG node when the
-- frontend has determined it is required.
--
-- This function will be called once for every unique selected build
-- configuration (that is, platform-debug, platform-release and so on).
--
-- The parameters are as follows:
--  - env - Private environment block for this node
--  - data - Type-checked data according to the blueprint below
--  - deps - Dependencies detected while setting up 'data', typically nil but
--           will be non-nil when generators are used in source lists.
function _examplegen:create_dag(env, data, deps)
	local full_fn = "$(OBJECTDIR)/_generated/" .. data.OutName
	return env:make_node {
		Label = "ExampleGen $(@)",
		Action = "$(EXAMPLEGEN) $(<) $(@)",
		Pass = data.Pass,
		Dependencies = deps,
		InputFiles = { data.Source },
		OutputFiles = { full_fn },
		ImplicitInputs = { "$(EXAMPLEGEN)" },
	}
end

-- Add a generator ExampleGenerator that accepts the following args:
--
-- Source: the source text file to read
-- OutName: the name of the generated C file
--
-- The parameters are described this way so they can be checked in the context
-- of the user's build script. This makes it possible to flag invalid
-- parameters as syntax errors with line number information.

local blueprint = {
	Source = { Required = true, Type = "string", Help = "Input filename", },
	OutName = { Required = true, Type = "string", Help = "Output filename", },
}

nodegen.add_evaluator("ExampleGenerator", _examplegen, blueprint)
