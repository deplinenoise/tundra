
Build {
	Configs = {
		Config {
			Name = "macosx-gcc",
			DefaultOnHost = "macosx",
			Tools = { "gcc-osx" },
		},
	},
	Units = function()
		SharedLibrary {
			Name = "slib",
			Sources = { "slib.c" },
		}

		Program {
			Name = "main",
			Depends = { "slib" },
			Sources = { "main.c" },
		}

		Default "main"
	end,
}
