
Program {
	Name = "generator",
	Pass = "CompileGenerator",
	Target = "$(MYGENERATOR)",
	Sources = { "generator.c" },
	ConfigRemap = {
		["macosx-mingw32"] = "macosx-gcc",
	},
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
