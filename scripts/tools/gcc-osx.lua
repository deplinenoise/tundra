local env = ...

-- load the generic GCC toolset first
load_toolset("gcc", env)

env:set_many {
	["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".m", ".a", ".o" },
	["FRAMEWORKS"] = "",
	["SHLIBOPTS"] = "-shared",
	["_OS_CCOPTS"] = "$(FRAMEWORKS:p-F)",
	["SHLIBCOM"] = "$(LD) $(SHLIBOPTS) $(LIBPATH:p-L) $(LIBS:p-l) $(FRAMEWORKS:p-framework ) -o $(@) $(<)",
	["PROGCOM"] = "$(LD) $(PROGOPTS) $(LIBS:p-l)  $(FRAMEWORKS:p-framework ) -o $(@) $(<)",
	["OBJCCOM"] = "$(CCCOM)", -- objc uses same commandline
}
