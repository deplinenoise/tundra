local env = ...

load_toolset("generic-dotnet", env)

-- C# support
env:set_many {
	["CSC"] = "csc",
	["CSPROGSUFFIX"] = ".exe",
	["CSLIBSUFFIX"] = ".dll",
	["CSRESGEN"] = "resgen $(<) $(@)",
	["_CSC_COMMON"] = "-warn:$(CSC_WARNING_LEVEL) /nologo $(CSLIBPATH:b:p/lib\\:) $(CSRESOURCES:b:p/resource\\:) $(CSLIBS:p/reference\\::A.dll)",
	["CSCLIBCOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:library -out:$(@:b) $(<:b)",
	["CSCEXECOM"] = "$(CSC) $(_CSC_COMMON) $(CSCOPTS) -target:exe -out:$(@:b) $(<:b)",
}
