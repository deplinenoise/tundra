local common = {
	Env = {
		MYGENERATOR = "$(OBJECTDIR)$(SEP)mygenerator$(PROGSUFFIX)",
	},
}

Build {
	Units = "units.lua",
	Passes = {
		CompileGenerator = { Name = "Compile Generator", BuildOrder = 1 },
		CodeGeneration = { Name = "Code Generation", BuildOrder = 2 },
	},
	SyntaxDirs = { "." },
	SyntaxExtensions = { "syntax" },
	Configs = {
		{
			Name = "macosx-gcc",
			Inherit = common,
			DefaultOnHost = "macosx",
			Tools = { "gcc" },
		},
		{
			Name = "macosx-mingw32",
			Inherit = common,
			Tools = { "gcc" },
			ReplaceEnv = {
				CC = "i386-mingw32-gcc",
				CXX = "i386-mingw32-g++",
				AR = "i386-mingw32-ar",
				LD = "i386-mingw32-gcc",
			},
		}
	},
}
