
DefRule {
	Name = "ExampleGenerator",
	Pass = "CodeGeneration",
	Command = "$(EXAMPLEGEN) $(<) $(@)",
	ImplicitInputs = { "$(EXAMPLEGEN)" },

	Blueprint = {
		Source = { Required = true, Type = "string", Help = "Input filename", },
		OutName = { Required = true, Type = "string", Help = "Output filename", },
	},

	Setup = function (env, data)
		return {
			InputFiles    = { data.Source },
			OutputFiles   = { "$(OBJECTDIR)/_generated/" .. data.OutName },
		}
	end,
}

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
		ExampleGenerator {
			Source = "hello.txt",
			OutName = "hello-generated.c"
		},
	},
}

Default "program"
