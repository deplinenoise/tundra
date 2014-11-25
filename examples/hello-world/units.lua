
Program {
	Name = "HelloWorld",
	Sources = { "hello.c" },
}

Default "HelloWorld"

Program {
   Name = "CXXHelloWorld",
   Sources = { "hello.cc" },
   ReplaceEnv = {
      LD = { "$(CXX)" ; Config = { "*-gcc-*", "*-clang-*" } },
   },
}

Default "CXXHelloWorld"
