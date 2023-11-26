module(..., package.seeall)

local vslatest = require "tundra.tools.msvc-latest"

function apply(env, options)
  local extra = {
    Clang = true
  }

  vslatest.apply(env, options, extra)

  env:set_many{
    ["CC"] = "clang-cl",
    ["CXX"] = "clang-cl",
    ["LD"] = "lld-link"
  }
end
