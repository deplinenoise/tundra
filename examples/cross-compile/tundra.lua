local common = {
	Env = {
		MYGENERATOR = "$(OBJECTDIR)$(SEP)mygenerator$(HOSTPROGSUFFIX)",
	},
}

Build {
	Units = "units.lua",
	Passes = {
		CompileGenerator = { Name = "Compile Generator", BuildOrder = 1 },
		CodeGeneration = { Name = "Code Generation", BuildOrder = 2 },
	},
	ScriptDirs = { "." },
	Configs = {
		Config {
			Name = "macosx-gcc",
			Inherit = common,
			DefaultOnHost = "macosx",
			Tools = { "gcc" },
		},
		Config {
			Name = "macosx-mingw32",
			Inherit = common,
			Virtual = true,
			Tools = { "gcc" },
			ReplaceEnv = {
				PROGSUFFIX = ".exe",
				SHLIBSUFFIX = ".dll",
				CC = "i386-mingw32-gcc",
				CXX = "i386-mingw32-g++",
				AR = "i386-mingw32-ar",
				LD = "i386-mingw32-gcc",
			},
		},
		Config {
			Name = "macosx-crosswin32",
			SubConfigs = {
				host = "macosx-gcc",
				target = "macosx-mingw32"
			},
			DefaultSubConfig = "target",
		},
	},
}
