
Program {
	Name = "generator",
	Pass = "CompileGenerator",
	Target = "$(EXAMPLEGEN)",
	Sources = { "generator.c" },
}

Default "generator"

Program {
	Name = "program",
	Sources = {
		"program.c",
		ExampleGenerator { Source = "hello.txt", OutName = "hello-generated.c" },
	},
}

Default "program"
