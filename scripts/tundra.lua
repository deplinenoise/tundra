module(..., package.seeall)

function main(...)
  local os = require "os"
  local package = require "package"

  local tundra_home = assert(os.getenv("TUNDRA_HOME"), "TUNDRA_HOME not set")

  -- Install script directories in package.path
  do
    local pp = package.path
    pp = pp .. ';' .. tundra_home .. "/scripts/?.lua;" .. tundra_home .. "/lua/etc/?.lua"
    package.path = pp
  end

  require "strict"

  local args = { }
  local action = select(1, ...)
  local build_script = select(2, ...)
  do
    local count = select('#', ...)
    for i = 3, count do
      args[#args + 1] = select(i, ...)
    end
  end

  local boot = require "tundra.boot"

  if action == 'generate-dag' then
    boot.generate_dag_data(build_script)
  elseif action == 'generate-ide-files' then
    boot.generate_ide_files(build_script, args)
  else
    print("unknown action " .. action)
    os.exit(1)
  end
end
