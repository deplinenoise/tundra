
require "support.syntax"

Program {
	Name = "generator",
	Pass = "CompileGenerator",
	Target = "$(MYGENERATOR)",
	Sources = { "generator.c" },
	SubConfig = "host",
}

Default "generator"

Program {
	Name = "program",
	Sources = {
		"program.c",
		MyGenerator { OutName = "a-generated-file.c" },
	},
}

Default "program"
