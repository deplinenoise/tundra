CSharpLib {
	Name = "MyLib",
	Sources = { "MyLib.cs" },
}

CSharpExe {
	Name = "csharp-shared",
	Sources = { "Main.cs" },
	Depends = { "MyLib" },
}

Default "csharp-shared"
