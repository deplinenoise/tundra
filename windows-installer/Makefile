
all:	Tundra.msi

VARS=-dScriptSrc=..\scripts 

CANDLE_OPTS=$(VARS) -ext WixUtilExtension -ext WixUIExtension -arch x64 -nologo
LIGHT_OPTS=$(VARS) -nologo -ext WixUtilExtension -ext WixUIExtension -cultures:en-us

Tundra.msi:	Tundra.wixobj ScriptFiles.wixobj VS2012Addin.wixobj
	light $(LIGHT_OPTS) -o $@ Tundra.wixobj ScriptFiles.wixobj VS2012Addin.wixobj

ScriptFiles.wxs:
	heat dir ..\scripts -dr APPLICATIONROOTDIRECTORY -cg ScriptFilesComponent -gg -sfrag -template fragment -var var.ScriptSrc -out $@

Tundra.wixobj: Tundra.wxs tundra-manual.html
	candle $(CANDLE_OPTS) -out $@ Tundra.wxs

VS2012Addin.wixobj: VS2012Addin.wxs
	candle $(CANDLE_OPTS) -out $@ VS2012Addin.wxs

ScriptFiles.wixobj: ScriptFiles.wxs
	candle $(CANDLE_OPTS) -out $@ ScriptFiles.wxs

tundra-manual.html:
	asciidoc.py -o $@ ..\doc\manual.asciidoc

clean:
	del /f Tundra.msi 2> nul
	del /f ScriptFiles.wxs 2> nul
	del /f *.wixobj 2> nul
	del /f Tundra.wixpdb 2> nul
	del /f tundra-manual.html 2> nul
