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
		{
			Name = "win32-winsdk",
			DefaultOnHost = "windows",
			Tools = { { "msvc-winsdk"; TargetArch = "x86" }, "yasm" },
			Env = {
				ASMOPTS = { "-f win32" },
			},
		},
	},
}

