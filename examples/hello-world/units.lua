
Program {
	Name = "HelloWorld",
	Sources = { "hello.c" },
}

Default "HelloWorld"

Program {
   Name = "CXXHelloWorld",
   Sources = { "hello.cc" },
   ReplaceEnv = {
      LD = { "$(C++)" ; Config = { "*-gcc-*" } },
   },
}

Default "CXXHelloWorld"