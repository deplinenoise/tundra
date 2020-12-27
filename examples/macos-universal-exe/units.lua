-- This program builds for ARM/x64 concretely
local test_prog = Program {
	Name = "test_prog",
	Sources = { "main.c" },
	Config = { "macos-arm-*", "macos-x64-*" },
}

-- This build step glues together programs
DefRule {
	Name = "Lipo",
	Annotation = "Lipo $(@)",
	Command = "lipo -create -output $(@) $(<)",
	Blueprint = {
		Programs = { Required = true, Type = "table", Help = "Programs to merge", },
		OutName = { Required = true, Type = "string", Help = "Output filename", },
	},
	Setup = function (env, data)
		local inputs = {}
		return {
			InputFiles    = data.Programs,
			OutputFiles   = { data.OutName },
		}
	end,
}

-- Glue together both output files. Because the files are referenced, tundra
-- will pull them into the build graph when the macos-uni config is built
local lipo_target = Lipo {
	-- This lipo target only builds in the universal config (and is the only thing there)
	Config = "macos-uni-*",
	Programs = {
		"$(OBJECTROOT)/macos-arm-$(CURRENT_VARIANT)-default/test_prog",
		"$(OBJECTROOT)/macos-x64-$(CURRENT_VARIANT)-default/test_prog",
	},
	OutName = "$(OBJECTDIR)/merged-test",
}

Default(lipo_target)

-- vim: noexpandtab ts=4 sw=4
