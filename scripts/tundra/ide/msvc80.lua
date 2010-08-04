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


-- Microsoft Visual Studio 2008 Solution/Project file generation

local _generator = ...
local nodegen = require"tundra.nodegen"
local util = require"tundra.util"
local native = require"tundra.native"

local LF = '\r\n'
local UTF_HEADER = '\239\187\191' -- byte mark EF BB BF 

function _generator:generate_files(config_tuples, raw_nodes, env)
	assert(config_tuples and #config_tuples > 0)

	self.msvc_platforms = {}
	for _, tuple in ipairs(config_tuples) do
		local variant, platform = tuple.Variant, tuple.Config.Name:match('^(%w-)%-')
		tuple.MsvcPlatform = platform:sub(1, 1):upper() .. platform:sub(2)
		tuple.MsvcName = variant .. "|" .. tuple.MsvcPlatform
		self.msvc_platforms[tuple.MsvcPlatform] = true
	end

	self.config_tuples = config_tuples

	if Options.Verbose then
		printf("Generating Visual Studio projects for %d configurations/variants", #config_tuples)
	end

	local projects = {}

	for _, unit in ipairs(raw_nodes) do
		projects[#projects + 1] = self:get_node_of(unit.Decl.Name)
	end

	if Options.Verbose then
		printf("%d projects to generate", #projects)
	end

	local base_dir = env:interpolate('$(OBJECTROOT)$(SEP)')

	local sln_file = base_dir .. "tundra-generated.sln" -- FIXME: pass in solution name
	self:generate_solution(sln_file, projects)

	for _, proj in ipairs(projects) do
		local vcproj_file = base_dir .. proj.Decl.Name .. ".vcproj"
		self:generate_project(vcproj_file, proj)
	end
end

function _generator:generate_solution(fn, projects)
	local sln = io.open(fn, 'wb')
	sln:write(UTF_HEADER, LF, "Microsoft Visual Studio Solution File, Format Version 10.00", LF, "# Visual Studio 2008", LF)

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

function _generator:generate_project(fn, project)
	local p = io.open(fn, 'wb')
	p:write('<?xml version="1.0" encoding="Windows-1252"?>', LF)
	p:write('<VisualStudioProject', LF)
	p:write('\tProjectType="Visual C++"', LF)
	p:write('\tVersion="9.00"', LF)
	p:write('\tName="', project.Decl.Name, '"', LF)
	p:write('\tProjectGUID="{', project.Guid, '}"', LF)
	p:write('\tKeyword="MakeFileProj"', LF)
	p:write('\tTargetFrameworkVersion="196613"', LF)
	p:write('\t>', LF)

	p:write('\t<Platforms>', LF)
	for platform, _ in pairs(self.msvc_platforms) do
		p:write('\t\t<Platform', LF)
		p:write('\t\t\tName="', platform, '"', LF)
		p:write('\t\t/>', LF)
	end
	p:write('\t</Platforms>', LF)

	p:write('\t<ToolFiles>', LF)
	p:write('\t</ToolFiles>', LF)

	p:write('\t<Configurations>', LF)
	for _, tuple in ipairs(self.config_tuples) do
		p:write('\t\t<Configuration', LF)
		p:write('\t\t\tName="', tuple.MsvcName, '"', LF)
		p:write('\t\t\tOutputDirectory="$(ConfigurationName)"', LF)
		p:write('\t\t\tIntermediateDirectory="$(ConfigurationName)"', LF)
		p:write('\t\t\tConfigurationType="0"', LF)
		p:write('\t\t>', LF)

		-- FIXME
		p:write('\t\t\t<Tool', LF)
		p:write('\t\t\t\tName="VCNMakeTool"', LF)
		p:write('\t\t\t\tBuildCommandLine=""', LF)
		p:write('\t\t\t\tReBuildCommandLine=""', LF)
		p:write('\t\t\t\tCleanCommandLine=""', LF)
		p:write('\t\t\t\tPreprocessorDefinitions=""', LF)
		p:write('\t\t\t\tIncludeSearchPath=""', LF)
		p:write('\t\t\t\tForcedIncludes=""', LF)
		p:write('\t\t\t\tAssemblySearchPath=""', LF)
		p:write('\t\t\t\tForcedUsingAssemblies=""', LF)
		p:write('\t\t\t\tCompileAsManaged=""', LF)
		p:write('\t\t\t/>', LF)

		p:write('\t\t</Configuration>', LF)
	end
	p:write('\t</Configurations>', LF)

	-- FIXME
	p:write('\t<References>', LF)
	p:write('\t</References>', LF)

	p:write('\t<Files>', LF)
	for _, fn in ipairs(util.flatten(project.Decl.Sources)) do
		if type(fn) == "string" then
			fn = fn:gsub('/', '\\')
			p:write('\t\t<File', LF)
			p:write('\t\t\tRelativePath="..\\', fn, '"', LF) -- FIXME: assumes certain output dir
			p:write('\t\t\t>', LF)
			p:write('\t\t</File>', LF)
		end
	end
	p:write('\t</Files>', LF)

	p:write('\t<Globals>', LF)
	p:write('\t</Globals>', LF)

	p:write('</VisualStudioProject>', LF)
	p:close()
end

do
	local types = { "Program", "StaticLibrary", "CSharpExe", "CSharpLib" } 
	for _, type_name in ipairs(types) do
		nodegen.add_evaluator(type_name, function (self, env, decl)
			local relative_fn = decl.Name .. ".vcproj"
			return {
				Decl = decl,
				Type = type_name,
				Sources = decl.Sources,
				RelativeFilename = relative_fn, 
				Filename = env:interpolate("$(OBJECTROOT)$(SEP)" .. relative_fn),
				Guid = native.digest_guid(decl.Name)
			}
		end)
	end
end
