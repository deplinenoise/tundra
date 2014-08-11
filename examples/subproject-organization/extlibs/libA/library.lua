
-- I want to set a searchpath that is used during compilation of the lib,
-- but also at the compilation of the program using this lib; but I also
-- want to write it only one time :)
local commonEnv = {
   CPPPATH = "."
}

ExternalLibrary {
    Name = "defaultConfigurationFromLibA",
    Propagate = {
        -- this should go into the main-app, but is used here as an example
        Libs = {Config="win32-*-*"; "User32.lib", "Gdi32.lib" },
    },
}

StaticLibrary {
   Name = "libA",
   Sources = {
      -- here you need to add the basepath
      _G.LIBROOT_LIBA .. "libHeader.h",
      _G.LIBROOT_LIBA .. "libImplementation.cpp",
   },
   Env = {
      commonEnv,
   },
   Propagate = {
      Env = {
        commonEnv,
      },
   },
}

-- if you want to generate own defrules you can define them here and write
-- helper functions to use them by inserting them into the global lua table:
--function _G.useDefRule(arg1, arg2)
--  return { defRule{Source = arg1; Output = arg2;}}
--end
