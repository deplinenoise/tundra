Build {
	Units = "units.lua",
	Passes= {
		PchGen = { Name = "Precompiled Header Generation", BuildOrder = 1 },
	},
	Configs = {
		{
			Name = "win32-msvc",
			DefaultOnHost = "windows",
			Tools = { "msvc-winsdk" },
		},
	},
}
