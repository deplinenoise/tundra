local native = require "tundra.native"

Build {

  Configs = {
    Config {
      Name = "foo-bar",
      DefaultOnHost = native.host_platform,
    },
  },

  Units = function()
    require "tundra.syntax.files"

    -- Copy a file with source/target that can vary per configuration 
    local copy_1 = CopyFile { Source = 'a', Target = '$(OBJECTDIR)/a' }

    -- Copy a file that is the same across all configurations
    local copy_2 = CopyFileInvariant { Source = 'a', Target = 'b' }

    Default(copy_1)
    Default(copy_2)
  end,

}
