-- Microsoft Visual Studio 2010 Solution/Project file generation

module(..., package.seeall)

local nodegen     = require "tundra.nodegen"
local util        = require "tundra.util"
local native      = require "tundra.native"
local msvc_common = require "tundra.ide.msvc-common"
local path        = require "tundra.path"

local LF = '\r\n'
local UTF_HEADER = '\239\187\191' -- byte mark EF BB BF 

local msvc_generator = {}
msvc_generator.__index = msvc_generator

local function slurp_file(fn)
  local fh, err = io.open(fn, 'rb')
  if fh then
    local data = fh:read("*all")
    fh:close()
    return data
  end
  return ''
end

local function replace_if_changed(new_fn, old_fn)
  local old_data = slurp_file(old_fn)
  local new_data = slurp_file(new_fn)
  if old_data == new_data then
    printf("No change for %s", old_fn)
    os.remove(new_fn)
    return
  end
  printf("Updating %s", old_fn)
  os.remove(old_fn)
  os.rename(new_fn, old_fn)
end

function msvc_generator:generate_solution(fn, projects)
  printf("Generating VS2012 solution %s", fn)
  local sln = io.open(fn .. '.tmp', 'wb')
  sln:write(UTF_HEADER, LF, "Microsoft Visual Studio Solution File, Format Version 12.00", LF, "# Visual Studio 2012", LF)

  -- Map folder names to array of projects under that folder
  local sln_folders = {}
  for _, proj in ipairs(projects) do
    local hints = proj.Decl.IdeGenerationHints
    local msvc_hints = hints and hints.Msvc or nil
    local folder = msvc_hints and msvc_hints.SolutionFolder or nil
    if folder then
      local projects = sln_folders[folder] or {}
      projects[#projects + 1] = proj
      sln_folders[folder] = projects
    end
  end

  for _, proj in ipairs(projects) do
    local name = proj.Decl.Name
    local fname = proj.RelativeFilename
    local guid = proj.Guid
    sln:write(string.format('Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "%s", "%s", "{%s}"', name, fname, guid), LF)
    sln:write('EndProject', LF)
  end

  for folder_name, _ in pairs(sln_folders) do
    local folder_guid = msvc_common.get_guid_string("folder/" .. folder_name)
    sln:write(string.format('Project("{2150E333-8FDC-42A3-9474-1A3956D46DE8}") = "%s", "%s", "{%s}"', folder_name, folder_name, folder_guid), LF)
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
      if proj.IsMeta then
        sln:write(leader, "Build.0 = ", tuple.MsvcName, LF)
      end
    end
  end
  sln:write("\tEndGlobalSection", LF)

  sln:write("\tGlobalSection(SolutionProperties) = preSolution", LF)
  sln:write("\t\tHideSolutionNode = FALSE", LF)
  sln:write("\tEndGlobalSection", LF)

  sln:write("\tGlobalSection(NestedProjects) = preSolution", LF)
  for folder_name, projects in pairs(sln_folders) do
    local folder_guid = msvc_common.get_guid_string("folder/" .. folder_name)
    for _, project in ipairs(projects) do
      sln:write(string.format('\t\t{%s} = {%s}', project.Guid, folder_guid), LF)
    end
  end
  sln:write("\tEndGlobalSection", LF)

  sln:write("EndGlobal", LF)
  sln:close()

  replace_if_changed(fn .. ".tmp", fn)
end

local function find_dag_node_for_config(project, tuple)
  local build_id = string.format("%s-%s-%s", tuple.Config.Name, tuple.Variant.Name, tuple.SubVariant)
  local nodes = project.Decl.__DagNodes
  if not nodes then
    return nil
  end

  if nodes[build_id] then
    return nodes[build_id]
  end
  errorf("couldn't find config %s for project %s (%d dag nodes) - available: %s",
    build_id, project.Decl.Name, #nodes, table.concat(util.table_keys(nodes), ", "))
end

function msvc_generator:generate_project(project, all_projects)
  local fn = project.Filename
  local p = io.open(fn, 'wb')
  p:write('<?xml version="1.0" encoding="utf-8"?>', LF)
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
  p:write('\t<PropertyGroup>', LF)
  p:write('\t\t<_ProjectFileVersion>10.0.30319.1</_ProjectFileVersion>', LF)
  p:write('\t</PropertyGroup>', LF)

  p:write('\t<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />', LF)

  -- Mark all project configurations as makefile-type projects
  for _, tuple in ipairs(self.config_tuples) do
    p:write('\t<PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'', tuple.MsvcName, '\'" Label="Configuration">', LF)
    p:write('\t\t<ConfigurationType>Makefile</ConfigurationType>', LF)
    p:write('\t\t<UseDebugLibraries>true</UseDebugLibraries>', LF) -- I have no idea what this setting affects
    p:write('\t\t<PlatformToolset>v110</PlatformToolset>', LF) -- I have no idea what this setting affects
    p:write('\t</PropertyGroup>', LF)
  end

  p:write('\t<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />', LF)

  for _, tuple in ipairs(self.config_tuples) do
    p:write('\t<PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'', tuple.MsvcName, '\'">', LF)

    local dag_node = find_dag_node_for_config(project, tuple)
    local include_paths, defines
    if dag_node then
      local env = dag_node.src_env
      local paths = util.map(env:get_list("CPPPATH"), function (p)
        local ip = env:interpolate(p)
        if not path.is_absolute(ip) then
          ip = native.getcwd() .. '\\' .. ip
        end
        return ip
      end)
      include_paths = table.concat(paths, ';')
      defines = env:interpolate("$(CPPDEFS:j;)")
    else
      include_paths = ''
      defines = ''
    end

    local root_dir    = native.getcwd()
    local build_id    = string.format("%s-%s-%s", tuple.Config.Name, tuple.Variant.Name, tuple.SubVariant)
    local base        = "\"" .. TundraExePath .. "\" -C \"" .. root_dir .. "\" "
    local build_cmd   = base .. build_id
    local clean_cmd   = base .. "--clean " .. build_id
    local rebuild_cmd = base .. "--rebuild " .. build_id

    if not project.IsMeta then
      build_cmd   = build_cmd .. " " .. project.Decl.Name
      clean_cmd   = clean_cmd .. " " .. project.Decl.Name
      rebuild_cmd = rebuild_cmd .. " " .. project.Decl.Name
    else
      local all_projs_str = table.concat(
        util.map(all_projects, function (p) return p.Decl.Name end), ' ')
      build_cmd   = build_cmd .. " " .. all_projs_str
      clean_cmd   = clean_cmd .. " " .. all_projs_str
      rebuild_cmd = rebuild_cmd .. " " .. all_projs_str
    end

    p:write('\t\t<NMakeBuildCommandLine>', build_cmd, '</NMakeBuildCommandLine>', LF)
    p:write('\t\t<NMakeOutput></NMakeOutput>', LF)
    p:write('\t\t<NMakeCleanCommandLine>', clean_cmd, '</NMakeCleanCommandLine>', LF)
    p:write('\t\t<NMakeReBuildCommandLine>', rebuild_cmd, '</NMakeReBuildCommandLine>', LF)
    p:write('\t\t<NMakePreprocessorDefinitions>', defines, ';$(NMakePreprocessorDefinitions)</NMakePreprocessorDefinitions>', LF)
    p:write('\t\t<NMakeIncludeSearchPath>', include_paths, ';$(NMakeIncludeSearchPath)</NMakeIncludeSearchPath>', LF)
    p:write('\t\t<NMakeForcedIncludes>$(NMakeForcedIncludes)</NMakeForcedIncludes>', LF)
    p:write('\t</PropertyGroup>', LF)
  end

  -- Emit list of source files
  p:write('\t<ItemGroup>', LF)
  for _, record in ipairs(project.Sources) do
    local path_str = assert(record.Path)
    if not path.is_absolute(path_str) then
      path_str = native.getcwd() .. '\\' .. path_str
    end
    p:write('\t\t<ClCompile Include="', path_str, '" />', LF)
  end
  p:write('\t</ItemGroup>', LF)

  p:write('\t<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />', LF)

  p:write('</Project>', LF)
  p:close()
end

local function get_common_dir(sources)
  local dir_tokens = {}
  for _, src in ipairs(sources) do
    local path = assert(src.Path)
    if not tundra.path.is_absolute(path) then
      local subdirs = {}
      for subdir in path:gmatch("([^\\\]+)\\") do
        subdirs[#subdirs + 1] = subdir
      end

      if #dir_tokens == 0 then
        dir_tokens = subdirs
      else
        for i = 1, #dir_tokens do
          if dir_tokens[i] ~= subdirs[i] then
            while #dir_tokens >= i do
              table.remove(dir_tokens)
            end
            break
          end
        end
      end
    end
  end

  local result = table.concat(dir_tokens, '\\')
  if #result > 0 then
    result = result .. '\\'
  end
  return result
end

function msvc_generator:generate_project_filters(project)
  local fn = project.Filename .. ".filters"
  local p = io.open(fn .. ".tmp", 'wb')
  p:write('<?xml version="1.0" encoding="Windows-1252"?>', LF)
  p:write('<Project')
  p:write(' ToolsVersion="4.0"')
  p:write(' xmlns="http://schemas.microsoft.com/developer/msbuild/2003"')
  p:write('>', LF)

  local common_dir = get_common_dir(util.filter(project.Sources, function (s) return not s.Generated end))
  local common_dir_gen = get_common_dir(util.filter(project.Sources, function (s) return s.Generated end))

  local filters = {}
  local sources = {}

  -- Mangle source filenames, and find which filters need to be created
  for _, record in ipairs(project.Sources) do
    local fn = record.Path
    local common_start = record.Generated and common_dir_gen or common_dir
    if fn:find(common_start, 1, true) then
      fn = fn:sub(#common_start+1)
    end

    local dir, filename = path.split(fn)

    if dir == '.' then
      dir = nil
    end

    local abs_path = record.Path
    if not path.is_absolute(abs_path) then
      abs_path = native.getcwd() .. '\\' .. abs_path
    end

    if record.Generated then
      dir = 'Generated Files'
    end

    sources[#sources + 1] = {
      FullPath  = abs_path,
      Directory = dir,
    }

    -- Register filter and all its parents
    while dir and dir ~= '.' do
      filters[dir] = true
      dir, _ = path.split(dir)
    end
  end

  -- Emit list of filters
  p:write('\t<ItemGroup>', LF)
  for filter_name, _ in pairs(filters) do
    if filter_name ~= "" then
      filter_guid = msvc_common.get_guid_string(filter_name)
      p:write('\t\t<Filter Include="', filter_name, '">', LF)
      p:write('\t\t\t<UniqueIdentifier>{', filter_guid, '}</UniqueIdentifier>', LF)
      p:write('\t\t</Filter>', LF)
    end
  end
  p:write('\t</ItemGroup>', LF)

  -- Emit list of source files
  p:write('\t<ItemGroup>', LF)
  for _, source in ipairs(sources) do
    if not source.Directory then
      p:write('\t\t<ClCompile Include="', source.FullPath, '" />', LF)
    end
  end
  for _, source in ipairs(sources) do
    if source.Directory then
      p:write('\t\t<ClCompile Include="', source.FullPath, '">', LF)
      p:write('\t\t\t<Filter>', source.Directory, '</Filter>', LF)
      p:write('\t\t</ClCompile>', LF)
    end
  end
  p:write('\t</ItemGroup>', LF)

  p:write('</Project>', LF)

  p:close()

  replace_if_changed(fn .. ".tmp", fn)
end

function msvc_generator:generate_project_user(project)
  local fn = project.Filename .. ".user"
  -- Don't overwrite user settings
  do
    local p, err = io.open(fn, 'rb')
    if p then
      p:close()
      return
    end
  end

  local p = io.open(fn, 'wb')
  p:write('<?xml version="1.0" encoding="utf-8"?>', LF)
  p:write('<Project')
  p:write(' ToolsVersion="4.0"')
  p:write(' xmlns="http://schemas.microsoft.com/developer/msbuild/2003"')
  p:write('>', LF)

  for _, tuple in ipairs(self.config_tuples) do
    local dag_node = find_dag_node_for_config(project, tuple)
    if dag_node then
      local exe = nil
      for _, output in util.nil_ipairs(dag_node.outputs) do
        if output:match("%.exe") then
          exe = output
          break
        end
      end
      if exe then
        p:write('\t<PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'', tuple.MsvcName, '\'">', LF)
        p:write('\t\t<LocalDebuggerCommand>', native.getcwd() .. '\\' .. exe, '</LocalDebuggerCommand>', LF)
        p:write('\t\t<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>', LF)
        p:write('\t\t<LocalDebuggerWorkingDirectory>', native.getcwd(), '</LocalDebuggerWorkingDirectory>', LF)
        p:write('\t</PropertyGroup>', LF)
      end
    end
  end

  p:write('</Project>', LF)

  p:close()
end
  
function msvc_generator:generate_files(ngen, config_tuples, raw_nodes, env, default_names, hints)
  assert(config_tuples and #config_tuples > 0)

  local solution_hints = hints and hints.MsvcSolutions
  if not solution_hints then
    print("No IdeGenerationHints.MsvcSolutions specified - using defaults")
    solution_hints = {
      ['tundra-generated.sln'] = {}
    }
  end

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
    local data = msvc_common.extract_data(unit, env, ".vcxproj")
    if data then
      projects[#projects + 1] = data
    end
  end
  
  local source_list = { { Path = "tundra.lua" } }
  local units = io.open("units.lua")
  if units then
    source_list[#source_list + 1] = { Path = "units.lua" }
    io.close(units)
  end

  if Options.Verbose then
    printf("%d projects to generate", #projects)
  end

  local base_dir = env:interpolate('$(OBJECTROOT)$(SEP)')

  native.mkdir(base_dir)

  local proj_lut = {}
  for _, p in ipairs(projects) do
    proj_lut[p.Decl.Name] = p
  end

  for name, data in pairs(solution_hints) do
    local sln_projects
    if data.Projects then
      sln_projects = {}
      for _, pname in ipairs(data.Projects) do
        local pp = proj_lut[pname]
        if not pp then
          errorf("can't find project %s for inclusion in %s -- check your MsvcSolutions data", pname, name)
        end
        sln_projects[#sln_projects + 1] = pp
      end
    else
      -- All the projects
      sln_projects = util.clone_array(projects)
    end

    local meta_name = "00-tundra-" .. path.drop_suffix(name)
    local meta_proj = {
      Decl = { Name = meta_name, IdeGenerationHints = { Msvc = { SolutionFolder = "Build System Meta" } } },
      Type = "meta",
      RelativeFilename = meta_name .. ".vcxproj",
      Filename = env:interpolate("$(OBJECTROOT)$(SEP)" .. meta_name .. ".vcxproj"),
      Sources = source_list,
      Guid = msvc_common.get_guid_string(meta_name),
      IsMeta = true,
    }

    local sln_file = base_dir .. name
    self:generate_project(meta_proj, sln_projects)
    self:generate_project_filters(meta_proj)
    self:generate_project_user(meta_proj)

    sln_projects[#sln_projects + 1] = meta_proj
    self:generate_solution(sln_file, sln_projects)
  end

  for _, proj in ipairs(projects) do
    self:generate_project(proj, projects)
    self:generate_project_filters(proj)
    self:generate_project_user(proj)
  end
end

nodegen.set_ide_backend(function(...)
  local state = setmetatable({}, msvc_generator)
  state:generate_files(...)
end)
