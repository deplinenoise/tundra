
Program {
   Name = "example-app",
   Sources = {
      "exampleThatUsesTheLib.cpp",
      --compileVertexShader("vs_cubes.sc", "vs_cubes.bin"),
      --compileFragmentShader("fs_cubes.sc", "fs_cubes.bin"),
   },
   Depends = {
      "libA", "defaultConfigurationFromLibA",
   },
   Env = {
      CPPPATH = {
         "extlibs/",
      },
   },
}
Default "example-app"
