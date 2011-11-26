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


-- Microsoft Visual Studio 2010 Solution/Project file generation

module(..., package.seeall)

local nodegen = require"tundra.nodegen"
local util = require"tundra.util"
local native = require"tundra.native"

local Options = tundra.boot.Options

local LF = '\r\n'
local UTF_HEADER = '\239\187\191' -- byte mark EF BB BF 

local msvc_generator = {}
msvc_generator.__index = msvc_generator

function msvc_generator:generate_solution(fn, projects)
	local sln = io.open(fn, 'wb')
	sln:write(UTF_HEADER, LF, "Microsoft Visual Studio Solution File, Format Version 11.00", LF, "# Visual Studio 2010", LF)

	for _, proj in ipairs(projects) do
		local name = proj.Decl.Name
		local fname = proj.RelativeFilename
		local guid = proj.Guid
		sln:write(string.format('Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "%s", "%s", "{%s}"', name, fname, guid), LF)
		sln:write('EndProject', LF)
	end

	sln:write("Global", LF)
	sln:write("\tGlobalSection(SolutionConfigurationPlatforms) = preSolution", LF)
	for _, tuple in ipairs(self.config_tuples) do
		sln:write(string.format('\t\t%s = %s', tuple.MsvcName, tuple.MsvcName), LF)
	end
	sln:write("\tEndGlobalSection", LF)

	sln:write("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution", LF)
	for _, proj in ipairs(projects) do
		for _, tuple in ipairs(self.config_tuples) do
			local leader = string.format('\t\t{%s}.%s.', proj.Guid, tuple.MsvcName)
			sln:write(leader, "ActiveCfg = ", tuple.MsvcName, LF)
			sln:write(leader, "Build.0 = ", tuple.MsvcName, LF)
		end
	end
	sln:write("\tEndGlobalSection", LF)

	sln:write("\tGlobalSection(SolutionProperties) = preSolution", LF)
	sln:write("\t\tHideSolutionNode = FALSE", LF)
	sln:write("\tEndGlobalSection", LF)

	sln:write("EndGlobal", LF)
	sln:close()
end

function msvc_generator:generate_project(project)
	local fn = project.Filename
	local p = io.open(fn, 'wb')
	p:write('<?xml version="1.0" encoding="Windows-1252"?>', LF)
	p:write('<Project')
	p:write(' DefaultTargets="Build"')
	p:write(' ToolsVersion="4.0"')
	p:write(' xmlns="http://schemas.microsoft.com/developer/msbuild/2003"')
	p:write('>', LF)

	-- List all project configurations
	p:write('\t<ItemGroup Label="ProjectConfigurations">', LF)
	for _, tuple in ipairs(self.config_tuples) do
		p:write('\t\t<ProjectConfiguration Include="', tuple.MsvcName, '">', LF)
		p:write('\t\t\t<Configuration>', tuple.MsvcConfiguration, '</Configuration>', LF)
		p:write('\t\t\t<Platform>', tuple.MsvcPlatform, '</Platform>', LF)
		p:write('\t\t</ProjectConfiguration>', LF)
	end
	p:write('\t</ItemGroup>', LF)

	p:write('\t<PropertyGroup Label="Globals">', LF)
	p:write('\t\t<ProjectGuid>{', project.Guid, '}</ProjectGuid>', LF)
	p:write('\t\t<Keyword>MakeFileProj</Keyword>', LF)
	p:write('\t</PropertyGroup>', LF)

	p:write('\t<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />', LF)

	-- Mark all project configurations as makefile-type projects
	for _, tuple in ipairs(self.config_tuples) do
		p:write('\t<PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'', tuple.MsvcName, '\'" Label="Configuration">', LF)
		p:write('\t\t<ConfigurationType>Makefile</ConfigurationType>', LF)
		p:write('\t\t<UseDebugLibraries>true</UseDebugLibraries>', LF) -- I have no idea what this setting affects
		p:write('\t</PropertyGroup>', LF)
	end

	p:write('\t<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />', LF)

	if project.IsMeta then
		-- Only generate build commands for the meta project

		for _, tuple in ipairs(self.config_tuples) do
			p:write('\t<PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'', tuple.MsvcName, '\'">', LF)

			local build_cmd = ""
			local clean_cmd = ""

			local root_dir = ".." -- FIXME
			local build_id = string.format("%s-%s-%s", tuple.Config.Name, tuple.Variant.Name, tuple.SubVariant)
			local base = "tundra -C " .. root_dir .. " "
			build_cmd = base .. build_id
			clean_cmd = base .. "-c " .. build_id

			p:write('\t\t<NMakeBuildCommandLine>', build_cmd, '</NMakeBuildCommandLine>', LF)
			p:write('\t\t<NMakeOutput>output_file_name</NMakeOutput>', LF)
			p:write('\t\t<NMakeCleanCommandLine>', clean_cmd, '</NMakeCleanCommandLine>', LF)
			p:write('\t\t<NMakeReBuildCommandLine>rebuild_command_line</NMakeReBuildCommandLine>', LF)
			p:write('\t\t<NMakePreprocessorDefinitions>preprocessor_definitions;WIN32;_DEBUG;$(NMakePreprocessorDefinitions)</NMakePreprocessorDefinitions>', LF)
			p:write('\t\t<NMakeIncludeSearchPath>include_search_path;$(NMakeIncludeSearchPath)</NMakeIncludeSearchPath>', LF)
			p:write('\t\t<NMakeForcedIncludes>forced_included_files;$(NMakeForcedIncludes)</NMakeForcedIncludes>', LF)
			p:write('\t</PropertyGroup>', LF)
		end
	end

	-- Emit list of source files
	p:write('\t<ItemGroup>', LF)
	for _, fn in ipairs(util.flatten(project.Sources)) do
		if type(fn) == "string" then
			fn = "..\\" .. fn:gsub('/', '\\') -- FIXME: assumes that the output dir is one dir down from tundra.lua
			p:write('\t\t<ClCompile Include="', fn, '" />', LF)
		end
	end
	p:write('\t</ItemGroup>', LF)

	p:write('\t<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />', LF)

	p:write('</Project>', LF)
	p:close()
end

function msvc_generator:generate_project_filters(project)
	local fn = project.Filename .. ".filters"
	local p = io.open(fn, 'wb')
	p:write('<?xml version="1.0" encoding="Windows-1252"?>', LF)
	p:write('<Project')
	p:write(' ToolsVersion="4.0"')
	p:write(' xmlns="http://schemas.microsoft.com/developer/msbuild/2003"')
	p:write('>', LF)

	local filters = {}
	local sources = {}

	-- Mangle source filenames, and find which filters need to be created
	for _, fn in ipairs(util.flatten(project.Sources)) do
		if type(fn) == "string" then
			fn = fn:gsub('/', '\\')
			local a, b, path, filename = string.find(fn, "(.*)\\(.*)")
			if filename == nil then
				filename = fn
			end

			sources[#sources + 1] = {
				PathAndFileName = "..\\" .. fn, -- FIXME: assumes that the output dir is one dir down from tundra.lua
				Path = path,
				FileName = filename,
			}

			-- Register filter and all its parents
			while path ~= nil do
				filters[path] = true
				a, b, path, filename = string.find(path, "(.*)\\(.*)")
			end
		end
	end

	-- Emit list of filters
	p:write('\t<ItemGroup>', LF)
	for filter_name, _ in pairs(filters) do
		filter_guid = native.digest_guid(filter_name)
		p:write('\t\t<Filter Include="', filter_name, '">', LF)
		p:write('\t\t\t<UniqueIdentifier>{', filter_guid, '}</UniqueIdentifier>', LF)
		p:write('\t\t</Filter>', LF)
	end
	p:write('\t</ItemGroup>', LF)

	-- Emit list of source files
	p:write('\t<ItemGroup>', LF)
	for _, source in ipairs(sources) do
		p:write('\t\t<ClCompile Include="', source.PathAndFileName, '" >', LF)
		if source.Path ~= nil then
			p:write('\t\t\t<Filter>', source.Path, '</Filter>', LF)
		end
		p:write('\t\t</ClCompile>', LF)
	end
	p:write('\t</ItemGroup>', LF)

	p:write('</Project>', LF)

	p:close()
end
	
function msvc_generator:generate_files(ngen, config_tuples, raw_nodes, env)
	assert(config_tuples and #config_tuples > 0)

	self.msvc_platforms = {}
	for _, tuple in ipairs(config_tuples) do
		local variant, platform = tuple.Variant, tuple.Config.Name:match('^(%w-)%-')
		local cased_platform = platform:sub(1, 1):upper() .. platform:sub(2)
		tuple.MsvcConfiguration = cased_platform .. " " .. variant.Name
		tuple.MsvcPlatform = "Win32" -- Platform has to be Win32, or else VS2010 Express tries to load support libs for the other platform
		tuple.MsvcName = tuple.MsvcConfiguration .. "|" .. tuple.MsvcPlatform
		self.msvc_platforms[tuple.MsvcPlatform] = true
	end

	self.config_tuples = config_tuples

	if Options.Verbose then
		printf("Generating Visual Studio projects for %d configurations/variants", #config_tuples)
	end

	local projects = {}

	for _, unit in ipairs(raw_nodes) do
		projects[#projects + 1] = ngen:get_node_of(unit.Decl.Name)
	end

	local meta_name = "00-Tundra"
	projects[#projects + 1] = {
		Decl = { Name = meta_name, },
		Type = "meta",
		RelativeFilename = meta_name .. ".vcxproj",
		Filename = env:interpolate("$(OBJECTROOT)$(SEP)" .. meta_name .. ".vcxproj"),
		Sources = { "tundra.lua" },
		Guid = native.digest_guid(meta_name),
		IsMeta = true,
	}

	if Options.Verbose then
		printf("%d projects to generate", #projects)
	end

	local base_dir = env:interpolate('$(OBJECTROOT)$(SEP)')

	local sln_file = base_dir .. "tundra-generated.sln" -- FIXME: pass in solution name
	self:generate_solution(sln_file, projects)

	for _, proj in ipairs(projects) do
		self:generate_project(proj)
		self:generate_project_filters(proj)
	end
end


function apply_nodegen(state)
	local types = { "Program", "SharedLibrary", "StaticLibrary", "CSharpExe", "CSharpLib", "ExternalLibrary" } 
	for _, type_name in ipairs(types) do
		nodegen.add_evaluator(type_name, function (generator, env, decl)
			local relative_fn = decl.Name .. ".vcxproj"
			local sources = nodegen.flatten_list("*-*-*-*", decl.Sources) or {}
			sources = util.filter(sources, function (x) return type(x) == "string" end)

			if decl.SourceDir then
				sources = util.map(sources, function (x) return decl.SourceDir .. x end)
			end

			sources = util.map(sources, function (x) return x:gsub('/', '\\') end)

			return {
				Decl = decl,
				Type = type_name,
				Sources = sources,
				RelativeFilename = relative_fn, 
				Filename = env:interpolate("$(OBJECTROOT)$(SEP)" .. relative_fn),
				Guid = native.digest_guid(decl.Name)
			}
		end)
	end

	nodegen.set_ide_backend(function(...)
		local state = setmetatable({}, msvc_generator)
		state:generate_files(...)
	end)
end
