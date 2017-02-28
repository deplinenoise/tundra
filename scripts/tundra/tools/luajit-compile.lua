module(..., package.seeall)

local path = require("tundra.path")
local depgraph = require("tundra.depgraph")
local gencpp = require("tundra.tools.generic-cpp")

local function compile_lua_file(env, pass, fn)
  return depgraph.make_node {
    Env = env,
    Label = 'Luajit Compile $(@)',
    Pass = pass,
    Action = "$(LUACOM)",
    InputFiles = { fn },
    OutputFiles = { path.make_object_filename(env, fn, '$(OBJECTSUFFIX)') },
  }
end

function apply(env, options)
  env:register_implicit_make_fn("lua", compile_lua_file)
  
  env:set_many {
	["LUA"] = "extern\\luajit\\bin64\\luajit.exe",
	["LUACOM"] = "$(LUA) -b $(LUAOPTS) $(<) $(@)",
	["LUAOPTS"] = "",
  }
end

