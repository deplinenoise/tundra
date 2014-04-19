Build {
	Units = "units.lua",
	Configs = {
		{
			Name = "macosx-gcc",
			DefaultOnHost = "macosx",
			Tools = { "gcc" },
		},
		{
			Name = "linux-gcc",
			DefaultOnHost = "linux",
			Tools = { "gcc" },
		},
		{
			Name = "win32-msvc",
			DefaultOnHost = "windows",
			Tools = { "msvc-vs2012" },
		},
		{
			Name = "win32-mingw",
			Tools = { "mingw" },
		},
	},
}
