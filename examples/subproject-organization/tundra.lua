
_G.LIBROOT_LIBA = "extlibs/libA/"

Build {
	Units = {
      "extlibs/libA/library.lua",
      "units.lua",
   	},
	Configs = {
		{
			Name = "macosx-gcc",
			DefaultOnHost = "macosx",
			Tools = { "gcc" },
			ReplaceEnv = {
				LD = {Config = { "*-gcc-*" }; "$(CXX)"},
			},
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
			Tools = { "msvc" },
		},
	},
}
