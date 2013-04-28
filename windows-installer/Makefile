
all:	Tundra.msi

VARS=-dScriptSrc=..\scripts 

CANDLE_OPTS=$(VARS) -arch x64 -nologo
LIGHT_OPTS=$(VARS) -nologo

Tundra.msi:	Tundra.wixobj ScriptFiles.wixobj
	light $(LIGHT_OPTS) -o $@ Tundra.wixobj ScriptFiles.wixobj

ScriptFiles.wxs:
	heat dir ..\scripts -dr APPLICATIONROOTDIRECTORY -cg ScriptFilesComponent -gg -sfrag -template fragment -var var.ScriptSrc -out $@

Tundra.wixobj: Tundra.wxs tundra-manual.html
	candle $(CANDLE_OPTS) -out $@ Tundra.wxs

ScriptFiles.wixobj: ScriptFiles.wxs
	candle $(CANDLE_OPTS) -out $@ ScriptFiles.wxs

tundra-manual.html:
	asciidoc.py -o $@ ..\doc\manual.asciidoc

clean:
	del /f Tundra.msi 2> nul
	del /f ScriptFiles.wxs 2> nul
	del /f ScriptFiles.wixobj 2> nul
	del /f Tundra.wixobj 2> nul
	del /f Tundra.wixpdb 2> nul
	del /f tundra-manual.html 2> nul
