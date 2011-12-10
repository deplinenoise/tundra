
local common = {
	Env = {
		EXAMPLEGEN = "$(OBJECTDIR)$(SEP)generator$(PROGSUFFIX)",
	},
}

Build {
	Units = "support/units.lua",

	ScriptDirs = { "." },

	Passes = {
		CompileGenerator = { Name="Compile generator", BuildOrder = 1 },
		CodeGeneration = { Name="Generate sources", BuildOrder = 2 },
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
