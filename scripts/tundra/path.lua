module(..., package.seeall)

local platform = require "tundra.platform"

function split(fn)
  local dir, file = fn:match("^(.*)[/\\]([^\\/]*)$")
  if not dir then
    return ".", fn
  else
    return dir, file
  end
end

local function tokenize_path(path)
  local result = {}
  for token in path:gmatch("[^/\\]+") do
    result[#result + 1] = {
      Str    = token,
      Drop   = token == '.',
      DotDot = token == '..',
    }
  end
  return result
end

function normalize(path)
  local segs = tokenize_path(path)

  -- Compute what segments to drop due to .. (dot dot) tokens
  local dotdot_drops = 0
  for i=#segs,1,-1 do
    local seg = segs[i]
    if seg.Drop then
      -- nothing
    elseif seg.DotDot then
      dotdot_drops = dotdot_drops + 1
      seg.Drop = true
    elseif dotdot_drops > 0 then
      dotdot_drops = dotdot_drops - 1
      seg.Drop = true
    end
  end

  -- Figure out what segments to concat
  local out_strs = {}
  for i = 1, dotdot_drops do
    out_strs[#out_strs + 1] = '..'
  end

  for _, seg in ipairs(segs) do
    if not seg.Drop then
      out_strs[#out_strs + 1] = seg.Str
    end
  end

  return table.concat(out_strs, SEP)
end

function join(dir, fn)
  return normalize(dir .. '/' .. fn)
end

function get_filename_dir(fn)
  return select(1, split(fn))
end

function get_filename(fn)
  return select(2, split(fn))
end

function get_extension(fn)
  return fn:match("(%.[^.]+)$") or ""
end

function drop_suffix(fn)
  assert(type(fn) == "string")
  return fn:match("^(.*)%.[^./\\]+$") or fn
end

function get_filename_base(fn)
  assert(fn, "nil filename")
  local _,_,stem = fn:find("([^/\\]+)%.[^.]*$")
  if stem then return stem end
  _,_,stem = fn:find("([^/\\]+)$")
  return stem
end

function make_object_filename(env, src_fn, suffix)
  local object_fn

  local src_suffix = get_extension(src_fn):sub(2)

  -- Drop leading $(OBJECTDIR)[/\\] in the input filename.
  do
    local pname = src_fn:match("^%$%(OBJECTDIR%)[/\\](.*)$")
    if pname then
      object_fn = pname
    else
      object_fn = src_fn
    end
  end

  -- Compute path under OBJECTDIR we want for the resulting object file.
  -- Replace ".." with "dotdot" to avoid creating files outside the
  -- object directory. Also salt the generated object name with the source
  -- suffix, so that multiple source files with the same base name don't end
  -- up clobbering each other (Tundra emits an error for this when checking
  -- the DAG)
  do
    local relative_name = drop_suffix(object_fn:gsub("%.%.", "dotdot"))
    object_fn = "$(OBJECTDIR)/$(UNIT_PREFIX)/" .. relative_name .. "__" .. src_suffix .. suffix
  end

  return object_fn
end

function is_absolute(v)
  local first = v:sub(1, 1)
  if first == "/" or first == "\\" then
    return true
  end

  if platform.host_platform() == "windows" then
    local sep = v:sub(2, 3)
    if sep == ':\\' or sep == ':/' then
      return true
    end
  end

  return false
end

-- remove_prefix( "src/include/", "src/include/abc.h" ) -> "abc.h"
function remove_prefix(prefix, fn)
  if fn:find(prefix, 1, true) == 1 then
    return fn:sub(#prefix + 1)
  else
    return fn
  end
end
