require "tundra.syntax.ispc"

Program {
	Name = "IspcDemo",
	Sources = {
		"main.c",
		ISPC {
      Pass   = "IspcGen",
      Source = "lanes.ispc"
    },
	},
}

Default "IspcDemo"
