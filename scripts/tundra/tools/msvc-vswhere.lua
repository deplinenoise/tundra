
module(..., package.seeall)

local native = require "tundra.native"

function apply_msvc_visual_studio(version, versionRange, env, options)
  if native.host_platform ~= "windows" then
    error("the msvc toolset only works on windows hosts")
  end
  
  tundra.unitgen.load_toolset('msvc', env)

  options = options or {}

  -- I simplified the bootstrapping a bit, 
  -- defaults are 64-bit and SDK version is 
  -- whatever the 2017 installation defaults to

  local target_arch = options.TargetArch or "x64"
  local host_arch = options.HostArch or "x64"
  local sdk_version = options.SdkVersion or "10.0.15063.0" 
  local vswhere_product = options.Product or "*"
  local installation_path = options.InstallationPath

  if installation_path == nil then
    -- query the VS installation service API for a version of VS that has the Visual C++ tooling
    -- only if Visual Studio was installed with the Visual C++ tooling will this find an edition
    
    -- for example, 
    -- if you install an edition (Community/Professional/Enterprise) of VS without the 
    -- desktop development with C++ vswhere won't use it
    
    -- https://github.com/Microsoft/vswhere/wiki/Examples
    -- https://github.com/Microsoft/vswhere/wiki/Find-VC

    -- Microsoft.VisualStudio.Product.Enterprise
    -- Microsoft.VisualStudio.Product.Professional
    -- Microsoft.VisualStudio.Product.Community
    -- Microsoft.VisualStudio.Product.BuildTools    
    -- https://docs.microsoft.com/en-us/visualstudio/install/workload-and-component-ids

    local vswhere = io.popen("vswhere -nologo -version " .. versionRange .. " -products " .. vswhere_product .. " -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>NUL")
    installation_path = vswhere:read()
    vswhere:close()
  end
   
  assert(installation_path, "Visual C++ [Version " .. version .. "] was not found/installed. Make sure vswhere (https://github.com/Microsoft/vswhere/releases) is in PATH or set InstallationPath to \"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\<Edition>\\\"")

  local vc_tools_version_file = io.open(installation_path .. "\\VC\\Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt")
  local vc_tools_version = vc_tools_version_file:read()
  assert(vc_tools_version, "cannot read file 'Microsoft.VCToolsVersion.default.txt'")
  vc_tools_version = vc_tools_version:match("[^%s]+") -- there's a trailing white space in that file :/
  vc_tools_version_file:close()

  local vc_tools = installation_path .. "\\VC\\Tools\\MSVC\\" .. vc_tools_version
  local vc_bin = vc_tools .. "\\bin\\Host" .. host_arch:upper() .. "\\" .. target_arch

  local cl_exe = '"' .. vc_bin .. "\\cl.exe" .. '"'
  local lib_exe = '"' .. vc_bin .. "\\lib.exe" .. '"'
  local link_exe = '"' .. vc_bin .. "\\link.exe" .. '"'

  env:set('CC', cl_exe)
  env:set('CXX', cl_exe)
  env:set('LIB', lib_exe)
  env:set('LD', link_exe)

  -- Force MSPDBSRV.EXE (fixes issues with cl.exe running in parallel and corrupting PDB files)
  env:set("CCOPTS", "/FS")
  env:set("CXXOPTS", "/FS")

  env:set_external_env_var('VSINSTALLDIR', installation_path)
  env:set_external_env_var('VCINSTALLDIR', installation_path .. "\\VC")

  local include = {}
  
  include[#include + 1] = vc_tools .. "\\include"

  env:set_external_env_var("INCLUDE", table.concat(include, ';'))

end
