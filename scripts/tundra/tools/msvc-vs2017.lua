
module(..., package.seeall)

local vswhere = require "tundra.tools.msvc-vswhere"

function apply(env, options)
  vswhere.apply_msvc_visual_studio("15.0", "[15.0,16.0)", env, options)
end
