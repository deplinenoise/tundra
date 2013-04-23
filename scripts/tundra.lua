local os = require "os"
local package = require "package"

local function main(...)
  _G.TundraRootDir = select(1, ...)
  _G.TundraExePath = select(2, ...)

  local args = { }
  local action = select(3, ...)
  local build_script = select(4, ...)
  do
    local count = select('#', ...)
    for i = 5, count do
      args[#args + 1] = select(i, ...)
    end
  end

  local boot = require "tundra.boot"
  require "strict"

  if action == 'generate-dag' then
    boot.generate_dag_data(build_script)
  elseif action == 'generate-ide-files' then
    boot.generate_ide_files(build_script, args)
  elseif action == 'unit-test' then
    require "tundra.selftest"
  else
    print("unknown action " .. action)
    os.exit(1)
  end
end

return {
    main = main
}
