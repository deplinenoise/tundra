
Program {
   Name = "main-app",
   Sources = {
      "main-app.cpp",

      -- explanation of the next line in extlibs/libA/library.lua
      --useDefRule("file1.source", "file1.dest"),
   },
   Depends = {
      -- here we need to include "defaultConfiguration", even if "libA" already has
      -- the dependency to the defaultConfiguration!
      "libA", "defaultConfigurationFromLibA",
   },
   Env = {
      CPPPATH = {
         "extlibs/",
      },
   },
}
Default "main-app"
