local common = {
  Env = {
    CPPPATH = { "." },

    CCOPTS = {
      -- clang and GCC
      { "-g"; Config = { "*-gcc-debug", "*-clang-debug" } },
      { "-g", "-O2"; Config = { "*-gcc-release", "*-clang-release" } },

      { "-Wall", "-Werror", "-Wextra", "-Wno-unused-parameter", "-Wno-unused-function";
         Config = { "*-gcc-*", "*-clang-*" }
       },
     },

    CPPDEFS = {
      { "NDEBUG"; Config = "*-*-release" },
    },
  },
}

Build {
  Configs = {
    Config {
      Name = "macosx-clang",
      Inherit = common,
      Tools = { "clang-osx" },
      DefaultOnHost = "macosx",
      SupportedOnHosts = { "macosx" },
    },
  },

  Variants = {
    { Name = "debug", Options = { GeneratePdb = true } },
    { Name = "release" },
  },

  DefaultVariant = "debug",

  Units = function ()
    local pkgconfig = require "tundra.syntax.pkgconfig"

    local png = pkgconfig.Configure("libpng", ExternalLibrary)

    local testprog = Program {
      Name = "testprog",
      Depends = { png },
      Sources = {
        "testprog.c",
      },
    }

    Default(testprog)
  end,
}
