
DefRule {
	Name = "MyGenerator",
	Pass = "CodeGeneration",
	Command = "$(MYGENERATOR) $(@)",
	ImplicitInputs = { "$(MYGENERATOR)" },
	Blueprint = {
		OutName = { Required = true, Type = "string" },
	},
	Setup = function (env, data)
		return {
			InputFiles = { },
			OutputFiles = { "$(OBJECTDIR)/_generated/" .. data.OutName },
		}
	end,
}

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
