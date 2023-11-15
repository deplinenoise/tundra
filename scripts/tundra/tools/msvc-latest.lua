module(..., package.seeall)

-- How does this work?
--
-- Visual Studio is installed in one of two locations depending on version and product
--   2017-2019 : C:\Program Files (x86)\Microsoft Visual Studio\<Version>\<Product>
--   2022-     : C:\Program Files\Microsoft Visual Studio\<Version>\<Product>
--
-- This will be the value for the VSINSTALLDIR environment variable.
--
-- Since it is possible to install any combination of Visual Studio products 
-- we have to check all of them. The first product with a VC tools version
-- will be used unless you ask for a specific product and/or VC tools version.
--
-- The VsDevCmd.bat script is used to initialize the Developer Command Prompt for VS.
-- It will unconditionally call the bat files inside "%VSINSTALLDIR%\Common7\Tools\vsdevcmd\core"
-- followed by "%VSINSTALLDIR%\Common7\Tools\vsdevcmd\ext" unless run with -no_ext.
-- 
-- The only two bat files that we care about are:
--   "%VSINSTALLDIR%\Common7\Tools\vsdevcmd\core\winsdk.bat"
--   "%VSINSTALLDIR%\Common7\Tools\vsdevcmd\ext\vcvars.bat"
--
-- And the rest of this is just reverse engineered from these bat scripts.

local native = require "tundra.native"
local native_path = require "tundra.native.path"

-- Note that while Community, Professional and Enterprise products are installed
-- in C:\Program Files while BuildTools are always installed in C:\Program Files (x86)
local vs_default_path = "C:\\Program Files (x86)\\Microsoft Visual Studio"

-- Add new Visual Studio versions here and update vs_default_version
local vs_default_paths = {
  ["2017"] = vs_default_path,
  ["2019"] = vs_default_path,
  ["2022"] = "C:\\Program Files\\Microsoft Visual Studio"
}

local vs_default_version = "2022"

local vs_products = {
  "BuildTools", -- default
  "Community",
  "Professional",
  "Enterprise"
}

-- dump keys of table into sorted array
local function keys(tbl)
  local ks, i = {}, 1
  for k, _ in pairs(tbl) do
    ks[i] = k
    i = i + 1
  end
  table.sort(ks)
  return ks
end

local supported_arch_mappings = {
  ["x86"] = "x86",
  ["x64"] = "x64",
  ["arm"] = "arm",
  ["arm64"] = "arm64",

  -- alias
  ["amd64"] = "x64"
}

local function get_arch(arch)
  local arch2 = supported_arch_mappings[arch:lower()]
  return arch2
end

local function get_arch_tuple(host_arch, target_arch)
  local host_arch2 = get_arch(host_arch)
  local target_arch2 = get_arch(target_arch)

  if host_arch2 == nil then
    error("unknown host architecture '" .. host_arch .. "' expected one of " ..
              table.concat(keys(supported_arch_mappings), ", "))
  end
  if target_arch2 == nil then
    error("unknown target architecture '" .. target_arch .. "' expected one of " ..
              table.concat(keys(supported_arch_mappings), ", "))
  end

  return host_arch2, target_arch2
end

local supported_app_platforms = {
  ["desktop"] = "Desktop", -- default
  ["uwp"] = "UWP",
  ["onecore"] = "OneCore"
}

local function get_app_platform(app_platform)
  local app_platform2 = supported_app_platforms[app_platform:lower()]
  if app_platform2 == nil then
    error("unknown target architecture '" .. app_platform .. "' expected one of " ..
              table.concat(keys(supported_app_platforms), ", "))
  end
  return app_platform2
end

local function find_winsdk(target_winsdk_version, app_platform)
  -- file:///C:/Program%20Files%20(x86)/Microsoft%20Visual%20Studio/2022/BuildTools/Common7/Tools/vsdevcmd/core/winsdk.bat#L63
  --   HKLM\SOFTWARE\Wow6432Node
  --   HKCU\SOFTWARE\Wow6432Node (ignored)
  --   HKLM\SOFTWARE             (ignored)
  --   HKCU\SOFTWARE             (ignored)
  local winsdk_key = "SOFTWARE\\Wow6432Node\\Microsoft\\Microsoft SDKs\\Windows\\v10.0"
  local winsdk_dir = native.reg_query("HKLM", winsdk_key, "InstallationFolder")
  local winsdk_versions = {}

  -- Due to the SDK installer changes beginning with the 10.0.15063.0
  local check_file = "winsdkver.h"
  if app_platform == "UWP" then
    check_file = "Windows.h"
  end

  local dirs, _, _ = native.list_directory(native_path.join(winsdk_dir, "Include"))
  for _, winsdk_version in ipairs(dirs) do
    if winsdk_version:find("^10.") then
      local testpath = native_path.join(winsdk_dir, "Include\\" .. winsdk_version .. "\\um\\" .. check_file)
      local test = native.stat_file(testpath)
      if not test.isdirectory and test.exists then
        winsdk_versions[#winsdk_versions + 1] = winsdk_version
      end
    end
  end

  if target_winsdk_version ~= nil then
    for _, winsdk_version in ipairs(winsdk_versions) do
      if winsdk_version == target_winsdk_version then
        return winsdk_dir, target_winsdk_version
      end
    end
    error("Windows SDK version '" .. target_winsdk_version .. "' not found.\n" ..
              "You can try one of these Windows SDK versions " .. table.concat(winsdk_versions, ", ") ..
              " or change the app platform to UWP")
  end

  return winsdk_dir, winsdk_versions[#winsdk_versions] -- latest 
end

local function find_vc_tools(vs_path, vs_version, vs_product, target_vc_tools_version, search_set)
  if vs_path == nil then
    if vs_product == "BuildTools" then
      vs_path = vs_default_path
    else
      vs_path = vs_default_paths[vs_version] or vs_default_paths[vs_default_version]
    end
  end

  -- file:///C:/Program%20Files%20(x86)/Microsoft%20Visual%20Studio/2022/BuildTools/Common7/Tools/vsdevcmd/ext/vcvars.bat#L729

  -- we ignore Microsoft.VCToolsVersion.v143.default.txt and use Microsoft.VCToolsVersion.default.txt 
  -- unless a specific VC tools version was requested

  local vs_install_dir = native_path.join(vs_path, vs_version .. "\\" .. vs_product)
  local vc_install_dir = native_path.join(vs_install_dir, "VC")
  local vc_tools_version = nil

  if target_vc_tools_version == nil then
    local f = io.open(vc_install_dir .. "\\Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt", "r")
    if f ~= nil then
      vc_tools_version = f:read("*l")
      f:close()
    end
  else
    vc_tools_version = target_vc_tools_version
  end

  if vc_tools_version ~= nil then
    local testpath = native_path.join(vc_install_dir, "Tools\\MSVC\\" .. vc_tools_version .. "\\include\\vcruntime.h")
    local test = native.stat_file(testpath)
    if not test.isdirectory and test.exists then
      return vs_install_dir, vc_install_dir, vc_tools_version
    end
  end

  search_set[#search_set + 1] = vs_install_dir
  return nil, nil, nil
end

function apply(env, options, extra)
  if native.host_platform ~= "windows" then
    error("the msvc toolset only works on windows hosts")
  end

  tundra.unitgen.load_toolset('msvc', env)

  options = options or {}
  extra = extra or {}

  -- these control the environment
  local vs_path = options.Path or options.InstallationPath
  local vs_version = options.Version or extra.Version or vs_default_version
  local vs_product = options.Product
  local host_arch = options.HostArch or "x64"
  local target_arch = options.TargetArch or "x64"
  local app_platform = options.AppPlatform or options.PlatformType or "Desktop" -- Desktop, UWP or OneCore
  local target_winsdk_version = options.WindowsSdkVersion or options.SdkVersion -- Windows SDK version
  local target_vc_tools_version = options.VcToolsVersion -- Visual C++ tools version
  local atl_mfc = options.AtlMfc or false

  if vs_default_paths[vs_version] == nil then
    print("Warning: Visual Studio " .. vs_version .. " has not been tested and might not work out of the box")
  end

  host_arch, target_arch = get_arch_tuple(host_arch, target_arch)
  app_platform = get_app_platform(app_platform)

  local env_path = {}
  local env_include = {}
  local env_lib = {}
  local env_lib_path = {} -- WinRT

  -----------------
  -- Windows SDK --
  -----------------

  -- file:///C:/Program%20Files%20(x86)/Microsoft%20Visual%20Studio/2022/BuildTools/Common7/Tools/vsdevcmd/core/winsdk.bat#L513

  local winsdk_dir, winsdk_version = find_winsdk(target_winsdk_version, app_platform)

  env_path[#env_path + 1] = native_path.join(winsdk_dir, "bin\\" .. winsdk_version .. "\\" .. host_arch)

  env_include[#env_include + 1] = native_path.join(winsdk_dir, "Include\\" .. winsdk_version .. "\\shared")
  env_include[#env_include + 1] = native_path.join(winsdk_dir, "Include\\" .. winsdk_version .. "\\um")
  env_include[#env_include + 1] = native_path.join(winsdk_dir, "Include\\" .. winsdk_version .. "\\winrt") -- WinRT (used by DirectX 12 headers)
  -- env_include[#env_include + 1] = native_path.join(winsdk_dir, "Include\\" .. winsdk_version .. "\\cppwinrt") -- WinRT

  env_lib[#env_lib + 1] = native_path.join(winsdk_dir, "Lib\\" .. winsdk_version .. "\\um\\" .. target_arch)

  -- We assume that the Universal CRT isn't loaded from a different directory
  local ucrt_sdk_dir = winsdk_dir
  local ucrt_version = winsdk_version

  env_include[#env_include + 1] = native_path.join(ucrt_sdk_dir, "Include\\" .. ucrt_version .. "\\ucrt")

  env_lib[#env_lib + 1] = native_path.join(ucrt_sdk_dir, "Lib\\" .. ucrt_version .. "\\ucrt\\" .. target_arch)

  -- Skip if the Universal CRT is loaded from the same path as the Windows SDK
  if ucrt_sdk_dir ~= winsdk_dir and ucrt_version ~= winsdk_version then
    env_lib[#env_lib + 1] = native_path.join(ucrt_sdk_dir, "Lib\\" .. ucrt_version .. "\\um\\" .. target_arch)
  end

  ----------------
  -- Visual C++ --
  ----------------

  local search_set = {}

  local vs_install_dir = nil
  local vc_install_dir = nil
  local vc_tools_version = nil

  -- If product is unspecified search for a suitable product
  if vs_product == nil then
    for _, product in pairs(vs_products) do
      vs_install_dir, vc_install_dir, vc_tools_version = find_vc_tools(vs_path, vs_version, product,
          target_vc_tools_version, search_set)
      if vc_tools_version ~= nil then
        vs_product = product
        break
      end
    end
  else
    vs_install_dir, vc_install_dir, vc_tools_version = find_vc_tools(vs_path, vs_version, vs_product,
        target_vc_tools_version, search_set)
  end

  if vc_tools_version == nil then
    local vc_product = "Visual C++ tools"
    local vc_product_version_disclaimer = ""
    if target_vc_tools_version ~= nil then
      vc_product = vc_product .. " [Version " .. target_vc_tools_version .. "]"
      vc_product_version_disclaimer =
          "Note that a specific version of the Visual C++ tools has been requested. Remove the setting VcToolsVersion if this was undesirable\n"
    end
    local vs_default_path = vs_default_paths[vs_default_version]:gsub("\\", "\\\\")
    error(vc_product .. " not found\n\n" .. "  Cannot find " .. vc_product ..
              " in any of the following locations:\n    " .. table.concat(search_set, "\n    ") ..
              "\n\n  Check that 'Desktop development with C++' is installed together with the product version in Visual Studio Installer\n\n" ..
              "  If you want to use a specific version of Visual Studio you can try setting Path, Version and Product like this:\n\n" ..
              "  Tools = {\n    { \"msvc-vs-latest\", Path = \"" .. vs_default_path .. "\", Version = \"" ..
              vs_default_version .. "\", Product = \"" .. vs_products[1] .. "\" }\n  }\n\n  " ..
              vc_product_version_disclaimer)
  end

  -- to do: extension SDKs?

  -- VCToolsInstallDir
  local vc_tools_dir = native_path.join(vc_install_dir, "Tools\\MSVC\\" .. vc_tools_version)

  -- VCToolsRedistDir
  -- Ignored for now. Don't have a use case for this

  -- file:///C:/Program%20Files%20(x86)/Microsoft%20Visual%20Studio/2022/BuildTools/Common7/Tools/vsdevcmd/ext/vcvars.bat#L707

  env_path[#env_path + 1] = native_path.join(vs_install_dir, "Common7\\IDE\\VC\\VCPackages")

  -- file:///C:/Program%20Files%20(x86)/Microsoft%20Visual%20Studio/2022/BuildTools/Common7/Tools/vsdevcmd/ext/vcvars.bat#L761

  env_path[#env_path + 1] = native_path.join(vc_tools_dir, "bin\\Host" .. host_arch .. "\\" .. target_arch)

  -- to do: IFCPATH? C++ header/units and/or modules?
  -- to do: LIBPATH? Fuse with #using C++/CLI
  -- to do: https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/intro-to-using-cpp-with-winrt#sdk-support-for-cwinrt

  env_include[#env_include + 1] = native_path.join(vs_install_dir, "VC\\Auxiliary\\VS\\include")
  env_include[#env_include + 1] = native_path.join(vc_tools_dir, "include")

  if app_platform == "Desktop" then
    env_lib[#env_lib + 1] = native_path.join(vc_tools_dir, "lib\\" .. target_arch)
    if atl_mfc then
      env_include[#env_include + 1] = native_path.join(vc_tools_dir, "atlmfc\\include")
      env_lib[#env_lib + 1] = native_path.join(vc_tools_dir, "atlmfc\\lib\\" .. target_arch)
    end
  elseif app_platform == "UWP" then
    -- file:///C:/Program%20Files%20(x86)/Microsoft%20Visual%20Studio/2022/BuildTools/Common7/Tools/vsdevcmd/ext/vcvars.bat#825
    env_lib[#env_lib + 1] = native_path.join(vc_tools_dir, "store\\" .. target_arch)
  elseif app_platform == "OneCore" then
    -- file:///C:/Program%20Files%20(x86)/Microsoft%20Visual%20Studio/2022/BuildTools/Common7/Tools/vsdevcmd/ext/vcvars.bat#830
    env_lib[#env_lib + 1] = native_path.join(vc_tools_dir, "lib\\onecore\\" .. target_arch)
  end

  -- Force MSPDBSRV.EXE (fix for issue with cl.exe running in parallel and otherwise corrupting PDB files)
  -- These options where added to Visual C++ in Visual Studio 2013. They do not exist in older versions.
  env:set("CCOPTS", "/FS")
  env:set("CXXOPTS", "/FS")

  env:set_external_env_var("VSINSTALLDIR", vs_install_dir)
  env:set_external_env_var("VCINSTALLDIR", vc_install_dir)
  env:set_external_env_var("INCLUDE", table.concat(env_include, ";"))
  env:set_external_env_var("LIB", table.concat(env_lib, ";"))
  env:set_external_env_var("LIBPATH", table.concat(env_lib_path, ";"))
  env:set_external_env_var("PATH", table.concat(env_path, ";"))

  -- Since there's a bit of magic involved in finding these we log them once, at the end.
  -- This also makes it easy to lock the SDK and C++ tools version if you want to do that.
  if target_winsdk_version == nil then
    print("  WindowsSdkVersion : " .. winsdk_version) -- verbose?
  end
  if target_vc_tools_version == nil then
    print("  VcToolsVersion    : " .. vc_tools_version) -- verbose?
  end
end
