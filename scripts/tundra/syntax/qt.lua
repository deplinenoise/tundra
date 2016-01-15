-- qt.lua -- support for Qt-specific compilers and tools

module(..., package.seeall)

local path = require "tundra.path"

DefRule {
  Name = "Moc",
  Command = "$(QTMOCCMD)",
  ConfigInvariant = true,
  Blueprint = {
    Source = { Required = true, Type = "string" },
  },

  Setup = function (env, data)
    local src = data.Source
    -- We follow these conventions:
    --   If the source file is a header, we do a traditional header moc:
    --     - input: foo.h, output: moc_foo.cpp
    --     - moc_foo.cpp is then compiled separately together with (presumably) foo.cpp
    --   If the source file is a cpp file, we do things a little differently:
    --     - input: foo.cpp, output foo.moc
    --     - foo.moc is then manually included at the end of foo.cpp
    local base_name = path.drop_suffix(src) 
    local pfx = 'moc_'
    local ext = '.cpp'
    if path.get_extension(src) == ".cpp" then
      pfx = ''
      ext = '.moc'
    end
    return {
      InputFiles = { src },
      OutputFiles = { "$(OBJECTDIR)$(SEP)" .. pfx .. base_name .. ext },
    }
  end,
}
