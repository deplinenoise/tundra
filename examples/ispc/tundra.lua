Build {
	Units = "units.lua",
	Passes = {
		IspcGen = { Name = "Generate ISPC .h files", BuildOrder = 1 },
	},
	Configs = {
		{
			Name = "win32-msvc",
			DefaultOnHost = "windows",
			Tools = { "msvc-vs2010", "ispc" },
			Env = {
				ISPCOPTS = {
					{ "--target=sse4 --cpu=corei7 --arch=x86";		Config = "win32-*" },
					{ "--target=sse4 --cpu=corei7 --arch=x86-64";	Config = "win64-*" },
				},
			},
			ReplaceEnv = {
				ISPC = "ispc.exe",	-- assume ispc.exe is in your path
			},
		},
	},
}
