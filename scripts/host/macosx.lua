local env = ...

env:set_many {
	["DOTNETRUN"] = "mono ",
	["PROGSUFFIX"] = "",
	["SHLIBSUFFIX"] = ".dylib",
}
