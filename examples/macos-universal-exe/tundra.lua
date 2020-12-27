local macos_x64 = {
	Env = {
		CCOPTS = { "-target x86_64-apple-macos10.12" },
		CXXOPTS = { "-target x86_64-apple-macos10.12" },
		PROGOPTS = { "-target x86_64-apple-macos10.12" },
	},
}

local macos_arm = {
	Env = {
		CCOPTS = { "-target arm64-apple-macos11" },
		CXXOPTS = { "-target arm64-apple-macos11" },
		PROGOPTS = { "-target arm64-apple-macos11" },
	},
}

Build {
	Units = "units.lua",
	Configs = { 
		Config {
			Name = "macos-arm",
			Inherit = macos_arm,
			Tools = { "clang" },
			SupportedHosts = { "macosx" },
		},

		Config {
			Name = "macos-x64",
			Inherit = macos_x64,
			Tools = { "clang" },
			SupportedHosts = { "macosx" },
		},

		Config {
			Name = "macos-uni",
			Tools = { "clang" },
			DefaultOnHost = "macosx",
			SupportedHosts = { "macosx" },
		},
	},

	Units = {
		"units.lua",
	},
}

-- vim: noexpandtab ts=4 sw=4




