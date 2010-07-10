-- test tundra build script

local util = require "tundra.util"

Passes = {
	Compile = { Name="Compile", BuildOrder = 1 },
}


local my_env = DefaultEnvironment:clone()

local liba = my_env.make.Library { Target = "$(OBJECTDIR)/lib1", Sources = { "a.c" } }
local libb = my_env.make.Library { Target = "$(OBJECTDIR)/lib2", Sources = { "b.c" } }

local both = my_env:make_node { Label = "Both",  Dependencies = { liba, libb } }

build(both)
