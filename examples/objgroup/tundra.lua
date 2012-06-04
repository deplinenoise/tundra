
Build {
	Units = function()

		local group1 = ObjGroup {
			Name = "foo",
			Sources = { "a.c", "b.c" }
		}

		local group2 = ObjGroup {
			Name = "bar",
			Sources = { "c.c" }
		}
		
		local testprog = Program {
			Name = "testprog",
			Depends = { "foo", group2 },
			Sources = {
				"main.c",
			}
		}

		Default(testprog)
	end,

	Configs = {
		{
			Name = "macosx-gcc",
			DefaultOnHost = "macosx",
			Tools = { "gcc" },
		},
	},
}
