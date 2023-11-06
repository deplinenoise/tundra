module(..., package.seeall)

local vslatest = require "tundra.tools.msvc-latest"

function apply(env, options)
  local extra = {
    Version = "2017"
  }
  vslatest.apply(env, options, extra)
end
