require "strict"

local os      = require "os"
local package = require "package"
local path    = require "tundra.path"

local function main(...)
  local cmd_args = { }
  for i = 1, select('#', ...) do
    cmd_args[#cmd_args + 1] = select(i, ...)
  end

  local action = assert(cmd_args[1], "need an action")

  local boot = require "tundra.boot"

  if action == 'generate-dag' or action == 'generate-ide-files' then
    local build_script = assert(cmd_args[2], "need a build script name")
    if action == 'generate-ide-files' then
      local ide_script = assert(cmd_args[3], "need a generator name")
      boot.generate_ide_files(build_script, ide_script)
    else
      boot.generate_dag_data(build_script)
    end
  elseif action == 'selftest' then
    require "tundra.selftest"
  else
    print("unknown action " .. action)
    os.exit(1)
  end
end

return {
    main = main
}
