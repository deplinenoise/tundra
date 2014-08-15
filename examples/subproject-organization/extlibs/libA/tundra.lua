
_G.LIBROOT_LIBA = "./"

Build {
	Units = {
		"library.lua",
		"example.lua",
	},
	Configs = {
		{
			Name = "macosx-gcc",
			DefaultOnHost = "macosx",
			Tools = { "gcc" },
		},
		{
			Name = "linux_x86-gcc",
			DefaultOnHost = "linux",
			Tools = { "gcc" },
			SupportedHosts = { "linux" },
			ReplaceEnv = {
				-- Link with the C++ compiler to get the C++ standard library.
				LD = "$(CXX)",
			},
		},
		{
			Name = "win32-msvc",
			DefaultOnHost = "windows",
			Tools = { "msvc-vs2013" },
		},
	},
}
