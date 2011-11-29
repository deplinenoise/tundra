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

-- msvc-winsdk.lua - Use Microsoft Windows SDK 7.1 or later to build.

module(..., package.seeall)

local native = require "tundra.native"
local os = require "os"

if native.host_platform ~= "windows" then
	error("the msvc toolset only works on windows hosts")
end
local sdk_key = "SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows"
local sdkDir = assert(native.reg_query("HKLM", sdk_key, "CurrentInstallFolder"))

local vc_key = "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VC7"
local vc_dir = assert(native.reg_query("HKLM", vc_key, "10.0"))
if vc_dir:sub(-1) ~= '\\' then
	vc_dir = vc_dir .. '\\'
end

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

local compiler_dirs = {
	["x86"] = {
		["x86"] = "bin\\",
		["x64"] = "bin\\x86_amd64\\",
		["itanium"] = "bin\\x86_ia64\\",
	},
	["x64"] = {
		["x86"] = "bin\\",
		["x64"] = "bin\\amd64\\",
		["itanium"] = "bin\\x86_ia64\\",
	},
	["itanium"] = {
		["x86"] = "bin\\x86_ia64\\",
		["itanium"] = "bin\\ia64\\",
	},
}

local function setup(env, options)
	options = options or {}
	local target_arch = options.TargetArch or "x86"
	local host_arch = options.HostArch or get_host_arch()

	local binDir = compiler_dirs[host_arch][target_arch]

	if not binDir then
		errorf("can't build target arch %s on host arch %s", target_arch, host_arch)
	end

	local cl_exe = '"' .. vc_dir .. binDir .. "cl.exe" ..'"'
	local lib_exe = '"' .. vc_dir .. binDir .. "lib.exe" ..'"'
	local link_exe = '"' .. vc_dir .. binDir .. "link.exe" ..'"'

	env:set('CC', cl_exe)
	env:set('CXX', cl_exe)
	env:set('LIB', lib_exe)
	env:set('LD', link_exe)

	-- Set up the MS SDK associated with visual studio

	env:set_external_env_var("WindowsSdkDir", sdkDir)
	env:set_external_env_var("INCLUDE", sdkDir .. "\\INCLUDE;" .. vc_dir .. "\\INCLUDE")

	local rc_exe = '"' .. sdkDir .. "\\bin\\rc.exe" ..'"'
	env:set('RC', rc_exe)
	env:set('RCOPTS', '/nologo')

	local sdkLibDir = "LIB"
	local vcLibDir = "LIB"

	if "x64" == target_arch then
		sdkLibDir = "LIB\\x64"
		vcLibDir = "LIB\\amd64"
	elseif "itanium" == target_arch then
		sdkLibDir = "LIB\\IA64"
		vcLibDir = "LIB\\IA64"
	end

	local libString = sdkDir .. "\\" .. sdkLibDir .. ";" .. vc_dir .. "\\" .. vcLibDir
	env:set_external_env_var("LIB", libString)
	env:set_external_env_var("LIBPATH", libString)

	local path = { }
	local vc_root = vc_dir:sub(1, -4)
	if binDir ~= "\\bin\\" then
		path[#path + 1] = vc_dir .. "\\bin"
	end
	path[#path + 1] = vc_root .. "Common7\\Tools" -- drop vc\ at end
	path[#path + 1] = vc_root .. "Common7\\IDE" -- drop vc\ at end
	path[#path + 1] = sdkDir
	path[#path + 1] = vc_dir .. binDir
	path[#path + 1] = env:get_external_env_var('PATH')

	env:set_external_env_var("PATH", table.concat(path, ';'))
end

function apply(env, options)
	-- Load basic MSVC environment setup first. We're going to replace the paths to
	-- some tools.
	tundra.boot.load_toolset('msvc', env)
	setup(env, options)
end
