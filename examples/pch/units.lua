require 'tundra.syntax.glob'

Program {
	Name = "testpch",
	PrecompiledHeader = {
		Source = "src/pch.cpp",
		Header = "pch.h",
		Pass = "PchGen",
	},
	Includes = {
		-- GCC and Clang need this for the PCH system to work. MSVC doesn't have that requirement.
		-- However, Tundra itself needs this, otherwise it's dependency header scanner doesn't work.
		-- So if you want to use a PCH, then you must include the directory containing the header file
		-- in your Includes list.
		"src",
	},
	SourceDir = "src/",
	Sources = {
		-- Note that it is OK to include pch.cpp in the Sources list (eg when using globbing), but it is not necessary
		--"pch.cpp",
		"main.cpp",
		"src1.cpp",
		"src2.cpp",
		"src3.cpp",
		"src4.cpp",
		"subdir/src5.cpp",
	},
}
Default "testpch"

-- This shows how to use precompiled headers with globbing.
-- It was initially used as a test to get globbing and precompiled headers working together correctly.
Program {
	Name = "testpch_glob",
	PrecompiledHeader = {
		Source = "src/pch.cpp",
		Header = "pch.h",
		Pass = "PchGen",
	},
	Includes = {
		"src",
	},
	Sources = {
		FGlob {
			Dir = "src",
			Extensions = { ".c", ".cpp" },
			Filters = {},
		}
	},
}
Default "testpch_glob"
