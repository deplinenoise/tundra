
Build {
	Configs = {
		Config {
			Name = "macosx-gcc",
			DefaultOnHost = "macosx",
			Tools = { "gcc-osx" },
		},
		Config {
			Name = "win64-msvc",
			DefaultOnHost = "windows",
			Tools = { { "msvc-vs2008"; TargetPlatform = "x64" } },
		},
	},
	Units = function()
		SharedLibrary {
			Name = "slib",
			Defines = { "SLIB_BUILDING", },
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
