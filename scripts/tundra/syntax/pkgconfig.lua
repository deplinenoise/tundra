module(..., package.seeall)

function ConfigureRaw(cmdline, name, constructor)
  local fh = assert(io.popen(cmdline))
  local data = fh:read("*all")
  fh:close()

  local cpppath = {}
  local libpath = {}
  local libs = {}
  local defines = {}

  for kind, value in data:gmatch("-([ILlD])([^ \n\r]+)") do
    if kind == "I" then
      cpppath[#cpppath + 1] = value
    elseif kind == "D" then
      defines[#defines + 1] = value
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
        CPPDEFS = defines,
        CPPPATH = cpppath,
        LIBS    = libs,
        LIBPATH = libpath
      }
    }
  })
end

function Configure(name, ctor)
  return internal_cfg("pkg-config " .. name .. " --cflags --libs", name, ctor)
end

function ConfigureWithTool(tool, name, ctor)
  return internal_cfg(tool .. " --cflags --libs", name, ctor)
end
