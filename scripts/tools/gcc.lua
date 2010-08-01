local env = ...

-- load the generic C toolset first
load_toolset("generic-cpp", env)

local native = require("tundra.native")

env:set_many {
	["NATIVE_SUFFIXES"] = { ".c", ".cpp", ".cc", ".cxx", ".a", ".o" },
	["OBJECTSUFFIX"] = ".o",
	["LIBSUFFIX"] = ".a",
	["CC"] = "gcc",
	["C++"] = "g++",
	["LIB"] = "ar",
	["LD"] = "gcc",
	["_OS_CCOPTS"] = "",
	["CCOPTS"] = "-Wall",
	["CCCOM"] = "$(CC) $(_OS_CCOPTS) -c $(CPPDEFS:p-D) $(CPPPATH:f:p-I) $(CCOPTS) $(CCOPTS_$(CURRENT_VARIANT:u)) -o $(@) $(<)",
	["CXXCOM"] = "$(CCCOM)",
	["PROGLIBS"] = "",
	["PROGOPTS"] = "",
	["PROGCOM"] = "$(LD) $(PROGOPTS) $(LIBS:p-l) -o $(@) $(<)",
	["LIBOPTS"] = "",
	["LIBCOM"] = "$(LIB) -r $(LIBOPTS) $(@) $(<)",
}
