unix_options = {
	Env = {
		CXXOPTS = {
			"-std=c++11",
		},
	},
}

Build {
	Units = "units.lua",
	Passes= {
		PchGen = { Name = "Precompiled Header Generation", BuildOrder = 1 },
	},
	Configs = {
		{
			Name = "win32-msvc",
			DefaultOnHost = "windows",
			Tools = { "msvc-vs2013" },
		},
		{
			Name = "linux-gcc",
			SupportedHosts = { "linux" },
			DefaultOnHost = "linux",
			Tools = { "gcc" },
			Inherit = { unix_options },
		},
		--{
		--	Name = "linux-clang",
		--	SupportedHosts = { "linux" },
		--	Tools = { "clang" },
		--	Inherit = { unix_options },
		--},
	},
}
