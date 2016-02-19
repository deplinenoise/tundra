local test_opts = {
	Env = {
		RUST_CARGO_OPTS = { 
			{ "test"; Config = "*-*-*-test" },
		},
	},
}

Build {
	Units = "units.lua",
	Configs = {
		{ 
			Name = "macosx-gcc",
			Inherit = test_opts,
			DefaultOnHost = "macosx",
			Tools = { "gcc-osx", "rust" },
		},
		{
			Name = "linux-gcc",
			Inherit = test_opts,
			DefaultOnHost = "linux",
			Tools = { "gcc", "rust" },
		},
		{
			Name = "freebsd-clang",
			Inherit = test_opts,
			DefaultOnHost = "freebsd",
			Tools = { "clang", "rust" },
		},
		{
			Name = "win64-msvc",
			Inherit = test_opts,
			DefaultOnHost = "windows",
			Tools = { "msvc", "rust" },
		},
	},

	Variants = { "debug", "release" },
	SubVariants = { "default", "test" },
}
