
Build {
	Configs = {
		Config {
			Name = "macosx-clang",
			DefaultOnHost = "macosx",
			Tools = { "clang-osx" },
		},
	},

	SyntaxExtensions = { "tundra.syntax.osx-bundle" },

	Units = "units.lua",
}
