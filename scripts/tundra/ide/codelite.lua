module(..., package.seeall)

local native  = require "tundra.native"
local nodegen = require "tundra.nodegen"
local path    = require "tundra.path"
local util    = require "tundra.util"
local ide_com = require "tundra.ide.ide-common"

local LF = "\n"
if native.host_platform == "windows" then
	LF = "\r\n"
end

local codelite_generator = {}
codelite_generator.__index = codelite_generator

nodegen.set_ide_backend(function(...)
  local state = setmetatable({}, codelite_generator)
  state:generate_files(...)
end)

local project_types    = ide_com.project_types
local binary_extension = ide_com.binary_extension
local header_exts      = ide_com.header_exts


local function get_common_dir(sources)
  local dir_tokens = {}
  local min_dir_tokens = nil
  for _, path in ipairs(sources) do
    if not tundra.path.is_absolute(path) then
      local subdirs = {}
      for subdir in path:gmatch("([^/]+)/") do
        subdirs[#subdirs + 1] = subdir
      end

      if not min_dir_tokens then
        dir_tokens = subdirs
        min_dir_tokens = #subdirs
      else
        for i = 1, #dir_tokens do
          if dir_tokens[i] ~= subdirs[i] then
            min_dir_tokens = (i - 1)
            while #dir_tokens >= i do
              table.remove(dir_tokens)
            end
            break
          end
        end
      end
    end
  end

  if(min_dir_tokens) then
    while #dir_tokens > min_dir_tokens do
      table.remove(dir_tokens)
    end
  end

  local result = table.concat(dir_tokens, SEP)
  if #result > 0 then
    result = result .. SEP
  end
  return result
end


function string:split(sep)
  local sep, fields = sep or "/", {}
  local pattern = string.format("([^%s]+)", sep)
  self:gsub(pattern, function(c) fields[#fields+1] = c end)
  return fields
end


local function get_files_by_folders(sources)
  files = {}
  
  for k, v in ipairs(sources) do
    files[#files+1] = v.Path
  end

  table.sort(files)
	
  local common_dir = get_common_dir(files)

  local function strip_common_dir(s)
    local first, second = s:find(common_dir)
    if 1 == first then
      return s:sub(second + 1)
    else
      return s
    end
  end

  -- strip common dir to avoid unnecessary virtual directories,
  -- re-prepend to actual file names later
  files = util.map(files, strip_common_dir)
  
  -- make sure we use absolute paths for filenames
  -- so Codelite can open files no matter where the
  -- project files are located
  if not path.is_absolute(common_dir) then
    local cwd = native.getcwd() .. "/"
    if common_dir ~= "" and common_dir:sub(-1) ~= "/" then
      common_dir = common_dir .. "/"
    end
    common_dir = cwd .. common_dir
  end

  result = {}

  for _, str in ipairs(files) do
    substrings = str:split()
    local tmp = result

    -- create a virtual directory for each directory
    for idx = 1, #substrings-1 do
      if not tmp[substrings[idx]] then
        tmp[substrings[idx]] = {}
      end
      tmp = tmp[substrings[idx]]
    end
    table.insert(tmp, common_dir .. str)
  end

  return result
end


local function make_meta_project(base_dir, data)
  data.IdeGenerationHints = { CodeLite = { SolutionFolder = "Build System Meta" } }
  data.IsMeta             = true
  data.RelativeFilename   = data.Name .. ".project"
  data.Filename           = base_dir .. data.RelativeFilename
  data.Type               = "meta"
  if not data.Sources then
    data.Sources          = {}
  end
  return data
end


local function make_project_data(units_raw, env, proj_extension, hints, ide_script)

  -- Filter out stuff we don't care about.
  local units = util.filter(units_raw, function (u)
    return u.Decl.Name and project_types[u.Keyword]
  end)

  local base_dir = (hints.CodeliteWorkspaceDir and (hints.CodeliteWorkspaceDir .. SEP)) or env:interpolate('$(OBJECTROOT)$(SEP)')
  native.mkdir(base_dir)

  local project_by_name = {}
  local all_sources  = {}
  local dag_node_lut = {} -- lookup table of all named, top-level DAG nodes 
  local name_to_dags = {} -- table mapping unit name to array of dag nodes (for configs)

  -- Map out all top-level DAG nodes
  for _, unit in ipairs(units) do
    local decl = unit.Decl

    local dag_nodes = assert(decl.__DagNodes, "no dag nodes for " .. decl.Name)
    for build_id, dag_node in pairs(dag_nodes) do
      dag_node_lut[dag_node] = unit
      local array = name_to_dags[decl.Name]
      if not array then
        array = {}
        name_to_dags[decl.Name] = array
      end
      array[#array + 1] = dag_node
    end
  end

  local function get_output_project(name)
    if not project_by_name[name] then
      local relative_fn = name .. proj_extension
      project_by_name[name] = {
        Name             = name,
        Sources          = {},
        RelativeFilename = relative_fn,
        Filename         = base_dir .. relative_fn,
      }
    end
    return project_by_name[name]
  end

  -- Sort units based on dependency complexity. We want to visit the leaf nodes
  -- first so that any source file references are picked up as close to the
  -- bottom of the dependency chain as possible.
  local unit_weights = {}
  for _, unit in ipairs(units) do
    local decl = unit.Decl
    local stack = { }
    for _, dag in pairs(decl.__DagNodes) do
      stack[#stack + 1] = dag
    end
    local weight = 0
    while #stack > 0 do
      local node = table.remove(stack)
      if dag_node_lut[node] then
        weight = weight + 1
      end
      for _, dep in util.nil_ipairs(node.deps) do
        stack[#stack + 1] = dep
      end
    end
    unit_weights[unit] = weight
  end

  table.sort(units, function (a, b)
    return unit_weights[a] < unit_weights[b]
  end)

  -- Keep track of what source files have already been grabbed by other projects.
  local grabbed_sources = {}

  for _, unit in ipairs(units) do
    local decl = unit.Decl
    local name = decl.Name

    local source_lut = {}
    local generated_lut = {}
    for build_id, dag_node in pairs(decl.__DagNodes) do
      ide_com.get_sources(dag_node, source_lut, generated_lut, 0, dag_node_lut)
    end

    -- Explicitly add all header files too as they are not picked up from the DAG
    -- Also pick up headers from non-toplevel DAGs we're depending on
    ide_com.get_headers(unit, source_lut, dag_node_lut, name_to_dags)

    -- Figure out which project should get this data.
    local output_name = name
    local ide_hints = unit.Decl.IdeGenerationHints
    if ide_hints then
      if ide_hints.OutputProject then
        output_name = ide_hints.OutputProject
      end
    end

    local proj = get_output_project(output_name)

    if output_name == name then
      -- This unit is the real thing for this project, not something that's
      -- just being merged into it (like an ObjGroup). Set some more attributes.
      proj.IdeGenerationHints = ide_hints
      proj.DagNodes           = decl.__DagNodes
      proj.Unit               = unit
    end

    for src, _ in pairs(source_lut) do
      local norm_src = path.normalize(src)
      if not grabbed_sources[norm_src] then
        grabbed_sources[norm_src] = unit
        local is_generated = generated_lut[src]
        proj.Sources[#proj.Sources+1] = {
          Path      = norm_src,
          Generated = is_generated,
        }
      end
    end
  end

  -- Get all accessed Lua files
  local accessed_lua_files = util.table_keys(get_accessed_files())

  -- Filter out the ones that belong to this build (exclude ones coming from Tundra) 
  local function is_non_tundra_lua_file(p)
    return not path.is_absolute(p)
  end
  local function make_src_node(p)
    return { Path = path.normalize(p) }
  end
  local source_list = util.map(util.filter(accessed_lua_files, is_non_tundra_lua_file), make_src_node)

  local solution_hints = hints.CodeliteWorkspace
  if not solution_hints then
    print("No IdeGenerationHints.CodeliteWorkspace specified - using defaults")
    solution_hints = {
      ['tundra-generated.workspace'] = {}
    }
  end

  local projects = util.table_values(project_by_name)
  local vanilla_projects = util.clone_array(projects)

  local solutions = {}

  -- Create meta project to regenerate solutions/projects. Added to every solution.
  local regen_meta_proj = make_meta_project(base_dir, {
    Name               = "00-Regenerate-Projects",
    FriendlyName       = "Regenerate Workspaces and Projects",
    BuildCommand       = ide_com.project_regen_commandline(ide_script),
  })

  projects[#projects + 1] = regen_meta_proj

  for name, data in pairs(solution_hints) do
    local sln_projects
    local ext_projects = {}
    if data.Projects then
      sln_projects = {}
      for _, pname in ipairs(data.Projects) do
        local pp = project_by_name[pname]
        if not pp then
          errorf("can't find project %s for inclusion in %s -- check your CodeliteWorkspace data", pname, name)
        end
        sln_projects[#sln_projects + 1] = pp
      end
    else
      -- All the projects (that are not meta)
      sln_projects = util.clone_array(vanilla_projects)
    end

    for _, ext in util.nil_ipairs(data.ExternalProjects) do
      ext_projects[#ext_projects + 1] = ext
    end

    -- Create meta project to build solution
    local meta_proj = make_meta_project(base_dir, {
        Name               = "00-tundra-" .. path.drop_suffix(name),
      FriendlyName       = "Build This Solution",
      BuildByDefault     = true,
      Sources            = source_list,
      BuildProjects      = util.clone_array(sln_projects),
    })

    sln_projects[#sln_projects + 1] = regen_meta_proj
    sln_projects[#sln_projects + 1] = meta_proj
    projects[#projects + 1]         = meta_proj

    -- workspacename will show up in Codelite and is used as the base name for the database file
    -- so we don't want the extension to be part of it
    local first, last = name:find(".workspace")
    local workspacename
    if first and first > 1 then
      workspacename = name:sub(1, first - 1)
    else
      workspacename = name
    end

    solutions[#solutions + 1] = {
        Workspacename        = workspacename,
      Filename             = base_dir .. name,
      Projects             = sln_projects,
      ExternalProjects     = ext_projects,
      BuildSolutionProject = meta_proj
    }
  end

  return solutions, projects
end


function codelite_generator:generate_workspace(fn, projects, ext_projects, workspace)
    local wrkspc = io.open(fn .. '.tmp', 'wb')

  -- Map folder names to array of projects under that folder
  local sln_folders = {}
  for _, proj in ipairs(projects) do
    local hints = proj.IdeGenerationHints
    local codelite_hints = hints and hints.Codelite or nil
    local folder = codelite_hints and codelite_hints.SolutionFolder or nil
    if folder then
      local projects = sln_folders[folder] or {}
      projects[#projects + 1] = proj
      sln_folders[folder] = projects
    end
  end

  local workspace_name = assert(workspace.Workspacename)

  wrkspc:write("<?xml version=\"1.0\" encoding=\"utf-8\"?>", LF)
  wrkspc:write("<CodeLite_Workspace Name=\"", workspace_name,
               "\" Database=\"", workspace_name, ".tags\">", LF)

  -- we simply activate the first project in the list,
  -- changes will be persisted by CodeLite anyway
  local is_active = "Yes"
  for _, proj in ipairs(projects) do
    local name = proj.Name
    local fname = proj.RelativeFilename

    wrkspc:write("  <Project Name=\"", name, "\" Path=\"", fname, "\" Active=\"", is_active, "\"/>", LF)
    is_active = "No"
  end

  wrkspc:write("  <BuildMatrix>", LF) 

  local selected_config = "yes"

  for _, tuple in ipairs(self.config_tuples) do
    wrkspc:write("    <WorkspaceConfiguration Name=\"", tuple.CodeliteConfiguration, "\" Selected=\"", selected_config, "\">", LF)
    for idx, proj in ipairs(projects) do
      wrkspc:write("      <Project Name=\"", proj.Name, "\" ConfigName=\"", tuple.CodeliteConfiguration, "\"/>", LF)
    end
    wrkspc:write("    </WorkspaceConfiguration>", LF)
    selected_config = "no"
  end

  wrkspc:write("  </BuildMatrix>", LF)
  wrkspc:write("</CodeLite_Workspace>", LF)

  wrkspc:close()

  ide_com.replace_if_changed(fn .. ".tmp", fn)
end


local function codelite_project_type(pt)

  -- Codelite may or may not make use of those strings
  -- so we just follow suit and make them match
  -- those in project files generated by Codelite
  local project_type  = "Executable"
  local internal_type = "Console"

  if type(pt) == "string" then
    if pt == "StaticLibrary" then
      project_type  = "Static Library"
      internal_type = "Console"
    elseif pt == "SharedLibrary" then
      project_type  = "Dynamic Library"
      internal_type = "Console"
    elseif pt == "Program" then
      project_type  = "Executable"
      internal_type = "Console"
    else
      -- Codelite doesn't provide CSharp support so we can treat CSharp projects as non-code projects in Codelite
      -- treat ExternalLibrary and ObjGroup as non-code projects, too
      project_type  = "Static Library"
      internal_type = ""
    end
  end
  return project_type, internal_type
end



function codelite_generator:generate_project(project, all_projects)
  local fn = project.Filename
  local p = assert(io.open(fn .. ".tmp", 'wb'))
  local project_type  = ""
  local internal_type = ""

  if project.Unit then
    project_type, internal_type = codelite_project_type(project.Unit.Keyword)
  end

  local src_files = get_files_by_folders(project.Sources)

  local function add_files_to_virtual_folders(p, files_by_folders, indent)
    assert(p)
    assert(indent)

    for k, v in pairs(files_by_folders) do
      if type(v) == "table" then
        assert(type(k) == "string")
        p:write(indent, "<VirtualDirectory Name=\"", k, "\">", LF)
        add_files_to_virtual_folders(p, v, indent .. "  ")
        p:write(indent, "</VirtualDirectory>", LF)
      else
        assert(type(v) == "string")
        -- adjust SEP in v if necessary, i.e. Win32
        local filename = v
        if SEP ~= "/" then
          filename = string.gsub(v, "/", SEP)
        end
        p:write(indent, "<File Name=\"", filename, "\"/>", LF)
      end
    end
  end

  p:write("<?xml version=\"1.0\" encoding=\"utf-8\"?>", LF)
  p:write("<CodeLite_Project Name=\"", project.Name, "\" InternalType=\"", "Console", "\">", LF)

  -- add the project's files
  local indent       = ""
  -- first figure out if we need to add a top level virtual folder;
  -- CodeLite requires one, it won't display files that don't reside inside a virtual folder
  local needs_folder = false
  for k, v in pairs(src_files) do
    if type(v) ~= "table" then
      needs_folder = true
      indent       = "  "
      break
    end
  end

  local vfolder_name = "src"
  local hints = project.IdeGenerationHints
  local codelite_hints = hints and hints.Codelite or nil
  if codelite_hints and codelite_hints.VFolder and (#codelite_hints.VFolder > 0) then
    vfolder_name = codelite_hints.VFolder
  end

  if needs_folder then
    p:write(indent, "<VirtualDirectory Name=\"" .. vfolder_name .. "\">", LF)
  end
  add_files_to_virtual_folders(p, src_files, indent .. "  ")
  if needs_folder then
    p:write(indent, "</VirtualDirectory>", LF)
  end

  p:write("  <Settings Type=\"", project_type, "\">", LF)

  for _, tuple in ipairs(self.config_tuples) do
    compiler = tuple.Config.Tools[1] or ""
    debugger = ""
    if compiler == "gcc" then
      compiler = "gnu gcc" -- will be ignored since we use tundra to build
      debugger = "GNU gdb debugger" -- probably used when debugging so we try to match the string Codelite expects
    end
    -- TODO
    -- adjust for other compilers

    local root_dir = native.getcwd()
    local build_id = string.format("%s-%s-%s", tuple.Config.Name, tuple.Variant.Name, tuple.SubVariant)

    local debugger_cmd = nil
    if project.Unit and project.Unit.Keyword == "Program" then
      local dag_node = ide_com.find_dag_node_for_config(project, tuple)
      local env = dag_node.src_env
      debugger_cmd = "Command=\"" .. root_dir .. SEP .. env:interpolate('$(OBJECTROOT)') .. SEP .. build_id .. SEP .. project.Name .. "\" "
    else
      debugger_cmd = "Command=\"\" "
    end

    p:write("    <Configuration Name=\"", tuple.CodeliteConfiguration, "\" CompilerType=\"", compiler,
            "\" DebuggerType=\"", debugger, "\" Type=\"", project_type,
            "\" BuildCmpWithGlobalSettings=\"append\" BuildLnkWithGlobalSettings=\"append\" ",
            "BuildResWithGlobalSettings=\"append\">", LF,
            "    <General OutputFile=\"\" IntermediateDirectory=\"\" ",
            debugger_cmd,
            "CommandArguments=\"\" UseSeparateDebugArgs=\"no\" DebugArguments=\"\" WorkingDirectory=\"\" PauseExecWhenProcTerminates=\"no\" IsGUIProgram=\"no\" IsEnabled=\"yes\"/>", LF)

    -- TODO add debugger config via IdeGenerationHints or use safe defaults
    -- Codelite will add empty config if missing:
--    p:write("      <Debugger IsRemote=\"no\" RemoteHostName=\"\" RemoteHostPort=\"\" DebuggerPath=\"\">", LF)
--    p:write("        <PostConnectCommands/>", LF)
--    p:write("        <StartupCommands/>", LF)
--    p:write("      </Debugger>", LF)

    p:write("      <CustomBuild Enabled=\"yes\">", LF)

    local base        = "\"" .. TundraExePath .. "\" -C \"" .. root_dir .. "\" "
    local build_cmd   = base .. build_id
    local clean_cmd   = base .. "--clean " .. build_id
    local rebuild_cmd = base .. "--rebuild " .. build_id

    -- This is needed in the case where a project maps to a different name using OutputProject
    local function get_project_actual_name(proj)
      if proj.Unit and proj.Unit.Decl then
        return proj.Unit.Decl.Name
      else
        return proj.Name
      end
    end

    project_name = get_project_actual_name(project)

    if project.BuildCommand then
      build_cmd = project.BuildCommand
      clean_cmd = ""
      rebuild_cmd = ""
    elseif not project.IsMeta then
      build_cmd   = build_cmd .. " " .. project_name
      clean_cmd   = clean_cmd .. " " .. project_name
      rebuild_cmd = rebuild_cmd .. " " .. project_name
    else
      local all_projs_str = table.concat(
        util.map(assert(project.BuildProjects), function (p) return get_project_actual_name(p) end), ' ')
      build_cmd   = build_cmd .. " " .. all_projs_str
      clean_cmd   = clean_cmd .. " " .. all_projs_str
      rebuild_cmd = rebuild_cmd .. " " .. all_projs_str
    end

    p:write("        <RebuildCommand>", rebuild_cmd, "</RebuildCommand>", LF)
    p:write("        <CleanCommand>", clean_cmd, "</CleanCommand>", LF)
    p:write("        <BuildCommand>", build_cmd, "</BuildCommand>", LF)
    p:write("        <PreprocessFileCommand/>", LF,
            "        <SingleFileCommand/>", LF,
            "        <MakefileGenerationCommand/>", LF,
            "        <ThirdPartyToolName/>", LF,
            "        <WorkingDirectory/>", LF)
    p:write("      </CustomBuild>", LF)
    -- TODO
    -- add CustomPreBuild and CustomPostBuild info
    -- add Completion info
    p:write("    </Configuration>", LF)
  end

  p:write("  </Settings>", LF)
  p:write("</CodeLite_Project>", LF)

  p:close()

  ide_com.replace_if_changed(fn .. ".tmp", fn)
end


function codelite_generator:generate_files(ngen, config_tuples, raw_nodes, env, default_names, hints, ide_script)
  assert(config_tuples and #config_tuples > 0)

  if not hints then
    hints = {}
  end

  local complained_mappings = {}

  local codelite_hints = hints.Codelite or {}

  local variant_mappings    = codelite_hints.VariantMappings    or {}
  local platform_mappings   = codelite_hints.PlatformMappings   or {}
  local subvariant_mappings = codelite_hints.SubVariantMappings or {}

  local friendly_names = {}

  for _, tuple in ipairs(config_tuples) do

    local friendly_name       = platform_mappings[tuple.Config.Name]  or tuple.Config.Name
    local friendly_variant    = variant_mappings[tuple.Variant.Name]  or tuple.Variant.Name
    local friendly_subvariant = subvariant_mappings[tuple.SubVariant] or tuple.SubVariant

    if #friendly_name == 0 then
      friendly_name = friendly_variant
    elseif #friendly_variant > 0 then -- Variant should not be empty, catch anyway
      friendly_name = friendly_name .. "-" .. friendly_variant
    end

    if #friendly_name == 0 then
      friendly_name = friendly_subvariant
    elseif #friendly_subvariant > 0 then
      friendly_name = friendly_name .. "-" .. friendly_subvariant
    end

    -- sanity check
    if #friendly_name == 0 then
      friendly_name = tuple.Config.Name .. '-' .. tuple.Variant.Name .. '-' .. tuple.SubVariant
    end

    if friendly_names[friendly_name] then
      -- duplicate friendly names will show up in Codelite and all seems well,
      -- however Codelite will always use the first match when building
      print("WARNING: friendly name '" .. friendly_name .. "' is not unique!")
    end
    friendly_names[friendly_name] = true

    tuple.CodeliteConfiguration = friendly_name
  end

  self.config_tuples = config_tuples

  printf("Generating Codelite projects for %d configurations/variants", #config_tuples)

  -- Figure out where we're going to store the projects
  local solutions, projects = make_project_data(raw_nodes, env, ".project", hints, ide_script)

  local proj_lut = {}
  for _, p in ipairs(projects) do
    proj_lut[p.Name] = p
  end

  for _, sln in pairs(solutions) do
    self:generate_workspace(sln.Filename, sln.Projects, sln.ExternalProjects, sln)
  end

  for _, proj in ipairs(projects) do
    self:generate_project(proj, projects)
  end
end
