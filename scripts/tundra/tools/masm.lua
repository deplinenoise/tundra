module(..., package.seeall)

function apply(env, options)
  -- load the generic assembly toolset first
  tundra.unitgen.load_toolset("generic-asm", env)

  env:set_many {
    ["MASM"] = "ml64.exe",
    ["ASMCOM"] = "$(MASM) /c /nologo $(ASMOPTS) /W3 /errorReport:none /Fo$(@) $(<)",
    ["ASMINC_KEYWORDS"] = { "include" },
  }
end
