
local common = {
	Env = {
		EXAMPLEGEN = "$(OBJECTDIR)$(SEP)generator$(PROGSUFFIX)",
	},
}

Build {
	Units = "tundra/units.lua",

	SyntaxDirs = { "tundra" },
	SyntaxExtensions = { "example-syntax" },

	Passes = {
		CompileGenerator = { Name="Compile generator", BuildOrder = 1 },
		CodeGeneration = { Name="Generate sources", BuildOrder = 2 },
	},

	Configs = {
		{
			Name = "macosx-gcc",
			DefaultOnHost = "macosx",
			Inherit = common,
			Tools = { "gcc", "mono" },
		},
	},
}
