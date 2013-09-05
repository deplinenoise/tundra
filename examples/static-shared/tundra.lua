Build {
  Env = {
    CPPDEFS = {
      { "SHARED"; Config = "*-*-*-shared" }
    },
  },
  Units = function ()
    local lib_static = StaticLibrary {
      Name = "foo_static",
      Config = "*-*-*-static",
      Sources = { "lib.c" },
    }

    local lib_shared = SharedLibrary {
      Name = "foo_shared",
      Config = "*-*-*-shared",
      Sources = { "lib.c" },
    }

    local prog = Program {
      Name = "bar",
      Depends = { lib_static, lib_shared },
      Sources = { "main.c" },
    }

    Default(prog)

  end,
  Configs = {
    {
      Name = "macosx-gcc",
      DefaultOnHost = "macosx",
      Tools = { "gcc" },
    },
    {
      Name = "linux-gcc",
      DefaultOnHost = "linux",
      Tools = { "gcc" },
    },
    {
      Name = "win32-msvc",
      DefaultOnHost = "windows",
      Tools = { "msvc-vs2012" },
    },
    {
      Name = "win32-mingw",
      Tools = { "mingw" },
      -- Link with the C++ compiler to get the C++ standard library.
      ReplaceEnv = {
        LD = "$(CXX)",
      },
    },
  },

  Variants = { "debug", "release" },
  SubVariants = { "static", "shared" },
}
