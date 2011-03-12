Build {
	Units = function ()
		Program {
			Name = "yasm-example",
			Sources = { "main.c", "example.asm" },
		}
		Default "yasm-example"
	end,
	Configs = {
		{
			Name = "macosx-gcc",
			DefaultOnHost = "macosx",
			Tools = { "gcc", "yasm" },
			Env = {
				CCOPTS = { "-m32" },
				PROGOPTS = { "-m32" },
				ASMOPTS = { "-f macho32" },
			},
		},
	},
}

