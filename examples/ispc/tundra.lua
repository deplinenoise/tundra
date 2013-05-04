
local common = {
  Env = {
    ISPCOPTS = {
      "--target=sse4 --cpu=corei7",
      { "--arch=x86"; Config = "win32-*" },
      { "--arch=x86-64";	Config = { "win64-*", "macosx-*" } },
    },
    CPPPATH = {
      -- Pick up generated ISPC headers from the object directory
      "$(OBJECTDIR)"
    }
  },
  ReplaceEnv = {
    ISPC = "ispc$(PROGSUFFIX)", -- assume ispc.exe is in your path
  },
}

Build {
	Units = "units.lua",
	Passes = {
		IspcGen = { Name = "Generate ISPC .h files", BuildOrder = 1 },
	},
	Configs = {
		Config {
			Name = "win32-msvc",
			DefaultOnHost = "windows",
			Tools = { "msvc-vs2010", "ispc" },
      Inherit = common,
		},
    Config {
			Name = "macosx-clang",
			DefaultOnHost = "macosx",
			Tools = { "clang-osx", "ispc" },
      Inherit = common,
    },
	},
}
