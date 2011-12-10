require "tundra.syntax.bison"

Build {
	Configs = {
		Config {
			Name = "generic-gcc",
			Tools = { "gcc" },
			Env = {
				BISON = "bison",
				BISONOPT = "",
				CPPPATH = "$(OBJECTDIR)", -- pick up generated files
			},
		},
	},
	Passes = {
		Codegen = { Name="Code generation", BuildOrder = 1 },
	},
	Units = function()
		local prog = Program {
			Name = "bison_example",
			Sources = {
				"main.c", "lexer.c",
				Bison {
					Source = "parser.y",
					Pass = "Codegen",
					TokenDefines = true,
				},
			}
		}

		Default(prog)
	end,
}
