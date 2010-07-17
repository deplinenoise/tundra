local env = ...

env:set_many {
	["CSLIBS"] = "", -- assembly references
	["CSCOPTS"] = "-optimize",
	["CSRESOURCES"] = "",
	["CSC_WARNING_LEVEL"] = "4",
}

