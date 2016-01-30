Build {
	Units = "units.lua",
	Configs = {
		{ 
			Name = "macosx-gcc",
			DefaultOnHost = "macosx",
			Tools = { "gcc-osx", "rust" },
		},
		{
			Name = "linux-gcc",
			DefaultOnHost = "linux",
			Tools = { "gcc", "rust" },
		},
		{
			Name = "freebsd-clang",
			DefaultOnHost = "freebsd",
			Tools = { "clang", "rust" },
		},
		{
			Name = "win64-msvc",
			DefaultOnHost = "windows",
			Tools = { "msvc", "rust" },
		},
	},
}
