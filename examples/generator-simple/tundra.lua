
local common = {
	Env = {
		GENSCRIPT = "generate-file.py",
	},
}

Build {
	Units = function()

		-- A rule to call out to a python file generator.
		DefRule {
			-- The name by which this rule will be called in source lists.
			Name               = "GenerateFile",

			-- All invocations of this rule will run in this pass unless
			-- overridden on a per-invocation basis.
			Pass               = "CodeGeneration",

			-- Mark the rule as configuration invariant, so the outputs are
			-- shared between all configurations. This is only suitable if the
			-- generator always generates the same outputs regardless of
			-- active configuration.
			ConfigInvariant    = true,

			-- The command to run.
			Command            = "python $(GENSCRIPT) $(<) $(@)",

			-- Add the generator script as an implicit input. This way, if you
			-- edit the script all your output files will be regenerated
			-- automatically the next time you build.
			ImplicitInputs     = { "$(GENSCRIPT)" },

			-- A blueprint to match against invocations. This provides error
			-- checking and exposes the data keys to the Setup function. 
			Blueprint = {
				Input = { Type = "string", Required = true },
				Output = { Type = "string", Required = true },
			},

			-- The Setup function must return a table of two keys InputFiles
			-- and OutputFiles based on the invocation data. It can optionally
			-- modify the environment (e.g. to add command switches based on
			-- the invocation data).
			Setup = function (env, data)
				return {
					InputFiles = { data.Input },
					OutputFiles = { "$(OBJECTROOT)/" .. data.Output },
				}
			end,
		}

		-- A test program that uses the file. Try running tundra for both debug
		-- and release at the same time and you will see that they share the
		-- generated file as an input without problems.
		local testprog = Program {
			Name = "testprog",
			Sources = {
				"main.c",
				GenerateFile { Input = "data.txt", Output = "data.c" },
			}
		}

		Default(testprog)
	end,

	Passes = {
		CodeGeneration = { Name="Generate sources", BuildOrder = 1 },
	},

	Configs = {
		{
			Name = "macosx-gcc",
			DefaultOnHost = "macosx",
			Inherit = common,
			Tools = { "gcc" },
		},
		{
			Name = "win32-msvc",
			DefaultOnHost = "windows",
			Inherit = common,
			Tools = { "msvc-vs2008" },
		},
	},
}
