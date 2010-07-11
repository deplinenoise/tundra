local env = ...

-- load the generic C toolset first
load_toolset("generic-cpp", env)

local native = require("tundra.native")

env:set_many {
	["OBJECTSUFFIX"] = ".o",
	["PROGSUFFIX"] = "",
	["LIBSUFFIX"] = ".a",
	["CC"] = "gcc",
	["C++"] = "g++",
	["LIB"] = "ar",
	["LD"] = "gcc",
	["CPPDEFS"] = "",
	["CCOPTS"] = "-Wall",
	["CCCOM"] = "$(CC) -c $(CPPDEFS:p-D) $(CPPPATH:f:p-I) $(CCOPTS) -o $(@) $(<)",
	["CXXCOM"] = "$(CCCOM)",
	["CSC"] = "gmcs",
	["CSCEXECOM"] = "$(CSC) /nologo /target:exe /warn:4 /optimize /out:$(@) $(<)",
	["PROGLIBS"] = "",
	["PROGOPTS"] = "",
	["PROGCOM"] = "$(LD) $(PROGOPTS) $(PROGLIBS) -o $(@) $(<)",
	["LIBOPTS"] = "",
	["LIBCOM"] = "$(LIB) $(LIBOPTS) $(@) $(<)",
}

