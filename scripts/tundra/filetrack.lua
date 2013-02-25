module(..., package.seeall)

-- Track accessed Lua files, for signature tracking. There's a Tundra-specific
-- callback that can be installed by calling set_loadfile_callback(). This
-- could improved if the Lua interpreted was changed to checksum files as they
-- were loaded and pass that hash here so we didn't have to open the files
-- again. OTOH, they should be in disk cache now.

local accessed_files = {}

function on_access(fn)
  accessed_files[fn] = true
end

set_loadfile_callback(on_access)

-- Also patch 'dofile', 'loadfile' to track files loaded without the package
-- facility.
do
  local old = { "dofile", "loadfile" }
  for _, name in ipairs(old) do
    local func = _G[name]
    _G[name] = function(fn, ...)
      on_access(fn)
      return func(fn, ...)
    end
  end
end

function get_accessed_files()
  return accessed_files
end
