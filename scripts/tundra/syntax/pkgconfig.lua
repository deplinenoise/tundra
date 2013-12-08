module(..., package.seeall)

function Configure(name, constructor)
  local fh = assert(io.popen("pkg-config " .. name .. " --cflags --libs"))
  local data = fh:read("*all")
  fh:close()

  local cpppath = {}
  local libpath = {}
  local libs = {}

  for kind, value in data:gmatch("-([ILl])([^ ]+)") do
    if kind == "I" then
      cpppath[#cpppath + 1] = value
    elseif kind == "L" then
      libpath[#libpath + 1] = value
    elseif kind == "l" then
      libs[#libs + 1] = value
    end
  end

  -- We don't have access to ExternalLibrary here - user has to pass it in.
  return constructor({
    Name = name,
    Propagate = {
      Env = {
        CPPPATH = cpppath,
        LIBS    = libs,
        LIBPATH = libpath
      }
    }
  })
end

