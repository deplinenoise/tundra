
Program {
	Name = "testpch",
	PrecompiledHeader = {
		Source = "src/pch.cpp",
		Header = "pch.h",
		Pass = "PchGen",
	},
	SourceDir = "src/",
	Sources = { "main.cpp", "src1.cpp" },
	--Sources = { "main.cpp", "src1.cpp", "pch.cpp" }, -- this also works. i.e. It's OK to duplicate the source file of the PCH here.
}

Default "testpch"
