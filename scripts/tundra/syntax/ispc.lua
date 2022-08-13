-- ispc.lua - Support for Intel SPMD Program Compiler

module(..., package.seeall)

local path = require "tundra.path"

DefRule {
  Name = "ISPC",
  Command = "$(ISPCCOM)",

  Blueprint = {
    Source = { Required = true, Type = "string" },
    Targets = { Required = false, Type = "string" },
  },

  Setup = function (env, data)
    local src = data.Source
    local base_name = path.drop_suffix(src) 
    local ext = path.get_extension(src):sub(2)
    local obj_file = "$(OBJECTDIR)$(SEP)" .. base_name .. "__" .. ext .. "$(OBJECTSUFFIX)"
    local h_file = "$(OBJECTDIR)$(SEP)" .. base_name .. "_ispc.h"
    -- Add the targets to the ISPC options if we have targets set
    if data.Targets ~= nil then
      env:append("ISPCOPTS", "--target=" .. data.Targets)
    end
    -- If we don't declare Targets or if it only contains one target we assume single output from ISPC
    if data.Targets == nil or not string.find(data.Targets, ",") then
      return {
        InputFiles = { src },
        OutputFiles = { obj_file, h_file },
      }
    else
      -- build a table with the targets we have without potentially width added (such as avx2-i16x16)
      local output_files = {}
      table.insert(output_files, obj_file)
      table.insert(output_files, h_file)

      for str in string.gmatch(data.Targets, "([^,]+)") do
        -- if name is target-<width> we skip the width specifiery when building the output name
        local target_name
        if string.find(str, "-") then
          target_name = string.sub(str, 1, string.find(str, "-")-1)
        else
          target_name = str
        end

        -- construct a name that looks like name__ispc_<target>.o/ and name_ispc_<target>.h
        local obj_file = "$(OBJECTDIR)$(SEP)" .. base_name .. "__" .. ext .. "_" ..  target_name .. "$(OBJECTSUFFIX)"
        local h_file = "$(OBJECTDIR)$(SEP)" .. base_name .. target_name .. "_ispc.h"

        table.insert(output_files, obj_file)
        table.insert(output_files, h_file)
      end

      return {
        InputFiles = { src },
        OutputFiles = output_files,
      }
    end
  end,
}
