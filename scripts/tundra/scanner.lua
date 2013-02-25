module(..., package.seeall)

local util   = require "tundra.util"

local _scanner_mt = {}
setmetatable(_scanner_mt, { __index = _scanner_mt })

local scanner_cache = {}

function make_cpp_scanner(paths)
  local key = table.concat(paths, '\0')

  if not scanner_cache[key] then
    local data = { Kind = 'cpp', Paths = paths, Index = #scanner_cache }
    scanner_cache[key] = setmetatable(data, _scanner_mt)
  end

  return scanner_cache[key]
end

function make_generic_scanner(data)
  error("fixme")
  data.Kind = 'generic'
  return setmetatable(data, _scanner_mt)
end

function all_scanners()
  local scanners = {}
  for k, v in pairs(scanner_cache) do
    scanners[v.Index + 1] = v
  end
  return scanners
end
