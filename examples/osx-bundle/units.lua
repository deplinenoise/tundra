
Program {
	Name = "foo",
	Sources = { "main.m", "delegate.m" },
	Frameworks = { "Cocoa" },
}

OsxBundle {
	Name = "mybundle",
	Depends = { "foo" },
	Target = "$(OBJECTDIR)/MyApp.app",
	InfoPList = "Info.plist",
	Executable = "$(OBJECTDIR)/foo",
	Resources = {
		CompileNib { Source = "appnib.xib", Target = "appnib.nib" },
		"icon.icns",
	},
}

Default "mybundle"
