local macos_x64 = {
	Env = {
		CCOPTS = { "-target x86_64-apple-macos10.12" },
		CXXOPTS = { "-target x86_64-apple-macos10.12" },
	},
}

local macos_arm = {
	Env = {
		CCOPTS = { "-target arm64-apple-macos11" },
		CXXOPTS = { "-target arm64-apple-macos11" },
	},
}

Build {
	Units = "units.lua",
	Configs = { 
		Config {
			Name = "macos-arm",
			Virtual = true,
			Inherit = macos_arm,
			Tools = { "clang" },
			SupportedHosts = { "macosx" },
		},

		Config {
			Name = "macos-x64",
			Inherit = macos_x64,
			Virtual = true,
			Tools = { "clang" },
			SupportedHosts = { "macosx" },
		},

		Config {
			Name = "macos-uni",
			DefaultOnHost = "macosx",
			SubConfigs = {
				host = "macos-x64",
				target = "macos-arm",
			},
			Tools = { "clang" },
			DefaultSubConfig = "target",
		},
	},

	Units = {
		"units.lua",
	},
}

-- vim: noexpandtab ts=4 sw=4




