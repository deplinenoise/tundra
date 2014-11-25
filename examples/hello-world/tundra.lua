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
			Name = "freebsd-clang",
			DefaultOnHost = "freebsd",
			Tools = { "clang" },
		},
		{
			Name = "win32-msvc",
			DefaultOnHost = "windows",
			Tools = { "msvc-vs2012" },
      Env = {
        CXXOPTS = { "/EHsc" },
      },
		},
		{
			Name = "win32-mingw",
			Tools = { "mingw" },
			-- Link with the C++ compiler to get the C++ standard library.
			ReplaceEnv = {
				LD = "$(CXX)",
			},
		},
	},
}
