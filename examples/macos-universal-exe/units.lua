Program {
	Name = "test",
	Sources = { "main.c" },
	-- SubConfig = { "host", "target" },
	Config = { "macos-arm", "macos-x64" },
}

Default "test"

-- vim: noexpandtab ts=4 sw=4
