
Program {
	Name = "HelloWorld",
  SourceDir = "a",
	Sources = {
    "hello.c",
    {
      "b.c";
      SourceDir = "a/b",
    }
  },
}

Default "HelloWorld"
