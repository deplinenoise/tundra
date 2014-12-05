-- icc.lua - Intel Compiler tool module (for windows)
module(..., package.seeall)

local vscommon = require "tundra.tools.msvc-vscommon"
local native = require "tundra.native"
local os = require "os"

-- Intel Composer tooling layout
local icc_bin_map = {
  ["x86"] = {
    ["x86"] = "ia32",
    ["x64"] = "intel64",
  },
  ["x64"] = {
    ["x86"] = "ia32",
    ["x64"] = "intel64",
  },
}

local icc_lib_map = {
  ["x86"] = {
    ["x86"] = "ia32",
    ["x64"] = "intel64",
  },
  ["x64"] = {
    ["x86"] = "ia32",
    ["x64"] = "intel64",
  },
}

local function get_host_arch()
  local snative = native.getenv("PROCESSOR_ARCHITECTURE")
  local swow = native.getenv("PROCESSOR_ARCHITEW6432", "")
  if snative == "AMD64" or swow == "AMD64" then
    return "x64"
  elseif snative == "IA64" or swow == "IA64" then
    return "itanium";
  else
    return "x86"
  end
end

local function path_combine(path, path_to_append)
  if path == nil then
    return path_to_append
  end
  if path:find("\\$") then
    return path .. path_to_append
  end
  return path .. "\\" .. path_to_append
end

local function path_it(maybe_list)
  if type(maybe_list) == "table" then
    return ipairs(maybe_list)
  end
  return ipairs({maybe_list})
end

function apply_icc(env, options)

  if native.host_platform ~= "windows" then
    error("the icc toolset only works on windows hosts for now")
  end
  
  -- Load basic MSVC environment setup first.
  -- We're going to replace the paths to some tools.
  tundra.unitgen.load_toolset('msvc', env)

  options = options or {}

  local target_arch = options.TargetArch or "x86"
  local host_arch = options.HostArch or get_host_arch()

  -- We'll find any edition of VS (including Express) here
  local icc_root = os.getenv("ICPP_COMPILER13") or os.getenv("ICPP_COMPILER12")
  assert(icc_root, "The requested version of Intel C Compiler isn't installed")
  icc_root = string.gsub(icc_root, "\\+$", "\\")

  local icc_bin = icc_bin_map[host_arch][target_arch]
  local icc_lib = icc_lib_map[host_arch][target_arch]
  
  if not icc_bin or not icc_lib then
    errorf("can't build target arch %s on host arch %s", target_arch, host_arch)
  end
  icc_bin =  icc_root .. "bin\\" .. icc_bin
  icc_lib =  icc_root .. "compiler\\lib\\" .. icc_lib

  --
  -- Tools
  --
  local cl_exe = '"' .. path_combine(icc_bin, "icl.exe") .. '"'
  local lib_exe = '"' .. path_combine(icc_bin, "xilib.exe") .. '"'
  local link_exe = '"' .. path_combine(icc_bin, "xilink.exe") .. '"'

  env:set('CC', cl_exe)
  env:set('CXX', cl_exe)
  env:set('LIB', lib_exe)
  env:set('LD', link_exe)

  local lib_str = env:get_external_env_var("LIB") .. ";" .. icc_lib
  env:set_external_env_var("LIB", lib_str)
  env:set_external_env_var("LIBPATH", lib_str)
  
  -- Modify %INCLUDE%
  local inc_str = icc_root .. "compiler\\include;" .. env:get_external_env_var("INCLUDE")
  env:set_external_env_var("INCLUDE", inc_str)

  -- Modify %PATH%
  local path = {}
  if "x86" == host_arch then
    path[#path + 1] = icc_root .. "bin\\ia32"
  elseif "x64" == host_arch then
    path[#path + 1] = icc_root .. "bin\\intel64"
  end
  path[#path + 1] = env:get_external_env_var('PATH') 
  env:set_external_env_var("PATH", table.concat(path, ';'))
   
  env:set("CCOPTS", "/Qwd1885 /Qwd597 /Qwd94 /Qwd810 /Qwd161 /Qwd2650")
  env:set("CXXOPTS", "/Qwd1885 /Qwd597 /Qwd94 /Qwd810 /Qwd161 /Qwd2650")
  return true
end

function apply(env, options)
  local vsvs = options.VsVersions or { "12.0", "11.0", "10.0", "9.0" }

  for _, v in ipairs(vsvs) do
  	local v1 = v
  	local success, result = xpcall(function() vscommon.apply_msvc_visual_studio(v1, env, options) end, function(err) return err end)
  	if success then
  		print("Visual Studio version " .. v1 .. " found ")
		-- Now adjust for the ICC tools...
		local success, result = pcall(apply_icc, env, options)
		if success then
			print("ICC found ")
			return
		else
			print("ICC not found (" .. result .. ")")
		end
  	else
  		print("Visual Studio version " .. v1 .. " does not appear to be installed (" .. result .. ")")
  	end
  end

  --error("Unable to find suitable version of Visual Studio and ICC (please install either version " .. table.concat(vsvs, ", ") .. " of Visual Studio to continue)")
  return false
end


