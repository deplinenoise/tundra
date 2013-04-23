require "strict"

local os      = require "os"
local package = require "package"
local path    = require "tundra.path"
local boot    = require "tundra.boot"

local actions = {
  ['generate-dag'] = function(build_script)
    assert(build_script, "need a build script name")
    boot.generate_dag_data(build_script)
  end,

  ['generate-ide-files'] = function(build_script, ide_script)
    assert(build_script, "need a build script name")
    assert(ide_script, "need a generator name")
    boot.generate_ide_files(build_script, ide_script)
  end,

  ['selftest'] = function()
    require "tundra.selftest"
  end
}

local function main(action_name, ...)
  assert(action_name, "need an action")

  local action = actions[action_name]
  assert(action, "unknown action")

  action(...)
end

return {
    main = main
}
