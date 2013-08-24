Build {
  Options = {
    MaxExpensiveJobs = 1,
  },

	Units = function ()
    for x = 0, 50 do
      local p = Program {
        Name = "p" .. tostring(x),
        Sources = "dummy.c",
      }
      Default(p)
    end
  end,

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
			-- Link with the C++ compiler to get the C++ standard library.
			ReplaceEnv = {
				LD = "$(CXX)",
			},
		},
	},
}
