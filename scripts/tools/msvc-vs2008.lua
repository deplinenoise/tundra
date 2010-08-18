-- Copyright 2010 Andreas Fredriksson
--
-- This file is part of Tundra.
--
-- Tundra is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- Tundra is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with Tundra.  If not, see <http://www.gnu.org/licenses/>.

-- msvc-vs2008.lua - Settings to use Microsoft Visual Studio 2008 from the
-- registry.

local env, options = ...

-- Load basic MSVC environment setup first. We're going to replace the paths to
-- some tools.
load_toolset('msvc', env)

local setup = toolset_once("msvc-vs2008", function()
	local native = require "tundra.native"
	local os = require "os"

	if native.host_platform ~= "windows" then
		error("the msvc toolset only works on windows hosts")
	end

	local vs9_key = "SOFTWARE\\Microsoft\\VisualStudio\\9.0"
	local idePath = assert(native.reg_query("HKLM", vs9_key, "InstallDir"))
	local rootDir = string.gsub(idePath, "\\Common7\\IDE\\$", "")

	local sdk_key = "SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows"
	local sdkDir = assert(native.reg_query("HKLM", sdk_key, "CurrentInstallFolder"))

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

	local arch_dirs = {
		["x86"] = {
			["x86"] = "\\vc\\bin\\",
			["x64"] = "\\vc\\bin\\x86_amd64\\",
			["itanium"] = "\\vc\\bin\\x86_ia64\\",
		},
		["x64"] = {
			["x86"] = "\\vc\\bin\\",
			["x64"] = "\\vc\\bin\\amd64\\",
			["itanium"] = "\\vc\\bin\\x86_ia64\\",
		},
		["itanium"] = {
			["x86"] = "\\vc\\bin\\x86_ia64\\",
			["itanium"] = "\\vc\\bin\\ia64\\",
		},
	}

	return function (env, options)
		options = options or {}
		local target_arch = options.TargetArch or "x86"
		local host_arch = options.HostArch or get_host_arch()

		local binDir = arch_dirs[host_arch][target_arch]

		if not binDir then
			errorf("can't build target arch %s on host arch %s", target_arch, host_arch)
		end

		local cl_exe = '"' .. rootDir .. binDir .. "cl.exe" ..'"'
		local lib_exe = '"' .. rootDir .. binDir .. "lib.exe" ..'"'
		local link_exe = '"' .. rootDir .. binDir .. "link.exe" ..'"'

		env:set('CC', cl_exe)
		env:set('C++', cl_exe)
		env:set('LIB', lib_exe)
		env:set('LD', link_exe)

		-- Expose the required variables to the external environment
		env:set_external_env_var('VSINSTALLDIR', rootDir)
		env:set_external_env_var('VCINSTALLDIR', rootDir .. '\\vc')
		env:set_external_env_var('DevEnvDir', idePath)

		-- Now look for MS SDK associated with visual studio

		env:set_external_env_var("WindowsSdkDir", sdkDir)
		env:set_external_env_var("INCLUDE", sdkDir .. "\\INCLUDE;" .. rootDir .. "\\VC\\ATLMFC\\INCLUDE;" .. rootDir .. "\\VC\\INCLUDE")

		local sdkLibDir = "LIB"
		local vcLibDir = "LIB"

		if "x64" == target_arch then
			sdkLibDir = "LIB\\x64"
			vcLibDir = "LIB\\amd64"
		elseif "itanium" == target_arch then
			sdkLibDir = "LIB\\IA64"
			vcLibDir = "LIB\\IA64"
		end

		local libString = sdkDir .. "\\" .. sdkLibDir .. ";" .. rootDir .. "\\VC\\ATLMFC\\" .. vcLibDir .. ";" .. rootDir .. "\\VC\\" .. vcLibDir
		env:set_external_env_var("LIB", libString)
		env:set_external_env_var("LIBPATH", libString)

		local path = { }
		path[#path + 1] = sdkDir
		path[#path + 1] = idePath

		if "x86" == host_arch then
			path[#path + 1] = rootDir .. "\\VC\\Bin"
		elseif "x64" == host_arch then
			path[#path + 1] = rootDir .. "\\VC\\Bin\\amd64"
		elseif "itanium" == host_arch then
			path[#path + 1] = rootDir .. "\\VC\\Bin\\ia64"
		end
		path[#path + 1] = rootDir .. "\\Common7\\Tools"

		path[#path + 1] = env:get_external_env_var('PATH') 

		env:set_external_env_var("PATH", table.concat(path, ';'))
	end
end)

setup(env, options)
