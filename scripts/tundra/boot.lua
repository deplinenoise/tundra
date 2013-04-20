module(..., package.seeall)

-- Use "strict" when developing to flag accesses to nil global variables
require "strict"

local os        = require "os"
local platform  = require "tundra.platform"
local util      = require "tundra.util"
local depgraph  = require "tundra.depgraph"
local unitgen   = require "tundra.unitgen"
local buildfile = require "tundra.buildfile"

-- This trio is so useful we want them everywhere without imports.
function _G.printf(msg, ...)
  local str = string.format(msg, ...)
  print(str)
end

function _G.errorf(msg, ...)
  local str = string.format(msg, ...)
  error(str)
end

function _G.croak(msg, ...)
  local str = string.format(msg, ...)
  io.stderr:write(str, "\n")
  os.exit(1)
end

local environment = require "tundra.environment"
local nodegen     = require "tundra.nodegen"
local decl        = require "tundra.decl"
local path        = require "tundra.path"
local depgraph    = require "tundra.depgraph"
local dagsave     = require "tundra.dagsave"

_G.SEP = platform.host_platform() == "windows" and "\\" or "/"

_G.Options = {
  FullPaths = 1
}

_G.TundraRootDir = assert(os.getenv("TUNDRA_HOME"), "TUNDRA_HOME not set")
_G.TundraExePath = assert(os.getenv("TUNDRA_EXECUTABLE"), "TUNDRA_EXECUTABLE not set")

local function make_default_env()
  local default_env = environment.create()

  default_env:set_many {
    ["OBJECTROOT"] = "t2-output",
    ["SEP"] = SEP,
  }

  do
    local mod_name = "tundra.host." .. platform.host_platform()
    local mod = require(mod_name)
    mod.apply_host(default_env)
  end

  return default_env
end

function generate_dag_data(build_script_fn)
  local build_data = buildfile.run(build_script_fn)

  local raw_nodes, node_bindings = unitgen.generate_dag(
    build_data.BuildTuples,
    build_data.BuildData,
    build_data.Passes,
    build_data.Configs,
    make_default_env())

  dagsave.save_dag_data(
    node_bindings,
    build_data.DefaultVariant,
    build_data.DefaultSubVariant)
end

function generate_ide_files(build_script_fn, args)
  -- We are generating IDE integration files. Load the specified
  -- integration module rather than DAG builders.
  --
  -- Also, default to using full paths for all commands to aid with locating
  -- sources better.
  Options.FullPaths = 1

  local build_data = buildfile.run(build_script_fn)

  local ide_script = assert(args[1], "no ide script specified")
  if not ide_script:find('.', 1, true) then
    ide_script = 'tundra.ide.' .. ide_script
  end
  require(ide_script)

  local build_tuples = assert(build_data.BuildTuples)
  local raw_data     = assert(build_data.BuildData)
  local passes       = assert(build_data.Passes)

  -- Generate dag
  local raw_nodes, node_bindings = unitgen.generate_dag(
    build_data.BuildTuples,
    build_data.BuildData,
    build_data.Passes,
    build_data.Configs,
    make_default_env())

  -- Pass the build tuples directly to the generator and let it write
  -- files.
  local env = make_default_env()
  nodegen.generate_ide_files(build_tuples, build_data.DefaultNodes, raw_nodes, env, build_data.BuildData.IdeGenerationHints)
end
