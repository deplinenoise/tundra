-- ispc.lua - Support for Intel SPMD Program Compiler

module(..., package.seeall)

local util     = require "tundra.util"
local nodegen  = require "tundra.nodegen"
local path     = require "tundra.path"
local depgraph = require "tundra.depgraph"

local _ispc_mt = nodegen.create_eval_subclass {}

function _ispc_mt:create_dag(env, data, deps)
  local src = data.Source
  --local objFile = path.make_object_filename(env, src, "$(OBJECTSUFFIX)")

  -- make_object_filename needs UNIT_PREFIX, which is not available in our environment here
  -- This is not perfect, but it's OK for now
  local objFile = "$(OBJECTDIR)$(SEP)" .. path.drop_suffix(src) .. "__" .. path.get_extension(src):sub(2) .. "$(OBJECTSUFFIX)"
  local hFile = path.drop_suffix(src) .. "_ispc.h"

  local pass = nil
  if data.Pass then
    pass = nodegen.resolve_pass(data.Pass)
  end
  if not pass then
    croak("%s: ISPC requires a valid Pass", data.Source)
  end

  return depgraph.make_node {
    Env          = env,
    Pass         = pass,
    Label        = "ISPC $(@)",
    Action       = "$(ISPCCOM)",
    InputFiles   = { data.Source },
    OutputFiles  = { objFile, hFile },
    Dependencies = deps,
  }
end

local ispc_blueprint = {
  Source = { Required = true, Type = "string" },
  Pass = { Required = true, Type = "string" },
}

nodegen.add_evaluator("ISPC", _ispc_mt, ispc_blueprint)
