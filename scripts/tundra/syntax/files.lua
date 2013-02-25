module(..., package.seeall)

local decl     = require "tundra.decl"
local nodegen  = require "tundra.nodegen"
local depgraph = require "tundra.depgraph"

function copy_file(env, source, target, pass, deps)
  return depgraph.make_node {
    Env          = env,
    Label        = "CopyFile $(@)",
    Action       = "$(_COPY_FILE)",
    Pass         = pass,
    InputFiles   = { source },
    OutputFiles  = { target },
    Dependencies = deps,
  }
end

function hardlink_file(env, source, target, pass, deps)
  return depgraph.make_node {
    Env          = env,
    Label        = "HardLink $(@)",
    Action       = "$(_HARDLINK_FILE)",
    Pass         = pass,
    InputFiles   = { source },
    OutputFiles  = { target },
    Dependencies = deps,
  }
end

local _copy_meta = { }

function _copy_meta:create_dag(env, data, deps)
  return copy_file(env, data.Source, data.Target, data.Pass, deps)
end

local _hardlink_meta = { }

function _hardlink_meta:create_dag(env, data, deps)
  return hardlink_file(env, data.Source, data.Target, data.Pass, deps)
end

local blueprint = {
  Source = {
    Importance = "required",
    Help = "Specify source filename",
    Type = "string",
  },
  Target = {
    Importance = "required",
    Help = "Specify target filename",
    Type = "string",
  },
}

nodegen.add_evaluator("CopyFile", _copy_meta, blueprint)
nodegen.add_evaluator("HardLinkFile", _hardlink_meta, blueprint)


