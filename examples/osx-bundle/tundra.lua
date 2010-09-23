
Build {
	Configs = {
		Config {
			Name = "macosx-clang",
			DefaultOnHost = "macosx",
			Tools = { "clang-osx" },
		},
	},

	SyntaxExtensions = { "osx-bundle" },

	Units = "units.lua",
}
