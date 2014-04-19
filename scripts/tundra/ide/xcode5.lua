-- Xcode 5 Workspace/Project file generation

module(..., package.seeall)

local path = require "tundra.path"
local nodegen = require "tundra.nodegen"
local util = require "tundra.util"
local native = require "tundra.native"

local xcode_generator = {}
local xcode_generator = {}
xcode_generator.__index = xcode_generator

function xcode_generator:generate_workspace(fn, projects)
  local sln = io.open(fn, 'wb')

  sln:write('<?xml version="1.0" encoding="UTF-8"?>\n')
  sln:write('<Workspace\n')
  sln:write('\tversion = "1.0">\n')

  for _, proj in ipairs(projects) do
    local name = proj.Decl.Name
    local fname = proj.RelativeFilename
    if fname == '.' then fname = '' 
    else fname = fname ..'/' 
    end
    sln:write('\t<FileRef\n')
    sln:write('\t\tlocation = "group:', name .. '.xcodeproj">\n')
    sln:write('\t</FileRef>\n')
  end

  sln:write('</Workspace>\n')
end

local project_types = util.make_lookup_table {
  "Program", "SharedLibrary", "StaticLibrary", 
}
local toplevel_stuff = util.make_lookup_table {
  ".exe", ".lib", ".dll",
}

local binary_extension = util.make_lookup_table {
  "", ".obj", ".o", ".a",
}

local header_exts = util.make_lookup_table {
  ".h", ".hpp", ".hh", ".inl",
}

local function newid(data)
  local string = native.digest_guid(data) 
  -- a bit ugly but is to match the xcode style of UIds
  return string.sub(string.gsub(string, '-', ''), 1, 24)
end

local function getfiletype(name)
  local types = {
    [".c"]         = "sourcecode.c.c",
    [".cc"]        = "sourcecode.cpp.cpp",
    [".cpp"]       = "sourcecode.cpp.cpp",
    [".css"]       = "text.css",
    [".cxx"]       = "sourcecode.cpp.cpp",
    [".framework"] = "wrapper.framework",
    [".gif"]       = "image.gif",
    [".h"]         = "sourcecode.c.h",
    [".html"]      = "text.html",
    [".lua"]       = "sourcecode.lua",
    [".m"]         = "sourcecode.c.objc",
    [".mm"]        = "sourcecode.cpp.objc",
    [".nib"]       = "wrapper.nib",
    [".pch"]       = "sourcecode.c.h",
    [".plist"]     = "text.plist.xml",
    [".strings"]   = "text.plist.strings",
    [".xib"]       = "file.xib",
    [".icns"]      = "image.icns",
    [""]           = "compiled.mach-o.executable",
  }
  return types[path.get_extension(name)] or "text"
end

-- Scan for sources, following dependencies until those dependencies seem to be a different top-level unit
local function get_sources(dag, sources, generated, dag_lut)
  for _, output in ipairs(dag.outputs) do
    local ext = path.get_extension(output)
    if not binary_extension[ext] then
      generated[output] = true
      sources[output] = true -- pick up generated headers
    end
  end

  for _, input in ipairs(dag.inputs) do
    local ext = path.get_extension(input)
    if not binary_extension[ext] then
      sources[input] = true
    end
  end

  for _, dep in util.nil_ipairs(dag.deps) do
    if not dag_lut[dep] then -- don't go into other top-level DAGs
      get_sources(dep, sources, generated, dag_lut)
    end
  end
end

local function get_headers(unit, sources, dag_lut, name_to_dags)
  local src_dir = ''

  if not unit.Decl then
    -- Ignore ExternalLibrary and similar that have no data.
    return
  end

  if unit.Decl.SourceDir then
    src_dir = unit.Decl.SourceDir .. '/'
  end
  for _, src in util.nil_ipairs(nodegen.flatten_list('*-*-*-*', unit.Decl.Sources)) do
    if type(src) == "string" then
      local ext = path.get_extension(src)
      if header_exts[ext] then
        local full_path = path.normalize(src_dir .. src)
        sources[full_path] = true
      end
    end
  end

  local function toplevel(u)
    if type(u) == "string" then
      return type(name_to_dags[u]) ~= "nil"
    end

    for _, dag in pairs(u.Decl.__DagNodes) do
      if dag_lut[dag] then
        return true
      end
    end
    return false
  end

  -- Repeat for dependencies ObjGroups
  for _, dep in util.nil_ipairs(nodegen.flatten_list('*-*-*-*', unit.Decl.Depends)) do
    if not toplevel(dep) then
      get_headers(dep, sources, dag_lut)
    end
  end
end

local function sort_filelist(source_list)
  local dest = {}
  for k, v in pairs(source_list) do table.insert(dest, { Key = k, Value = v }) end
  table.sort(dest, function(a, b) return a.Value < b.Value end)
  return dest
end

local function write_file_refs(p, source_list)
  p:write('/* Begin FBXFileReference section */\n')
  local cwd = native.getcwd();
  
  for _, entry in pairs(sort_filelist(source_list)) do
    local key = entry.Key
    local fn = entry.Value 
    local name = path.get_filename(fn) 
    local file_type = getfiletype(fn)
    local str = ""
    if file_type == "compiled.mach-o.executable" then
      str = string.format('\t\t%s /* %s */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = %s; name = "%s"; includeInIndex = 0; path = "%s"; sourceTree = BUILT_PRODUCTS_DIR; };',
        key, fn, file_type, name, fn)
    else
      str = string.format('\t\t%s /* %s */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = %s; name = "%s"; path = "%s"; sourceTree = "<group>"; };',
        key, fn, file_type, name, path.join(cwd, fn))
    end
    p:write(str, '\n')
  end

  p:write('/* End FBXFileReference section */\n\n')
end

local function write_legacy_target(p, project)
  local name = project.Decl.Name
  --[[
  isa = PBXLegacyTarget;
  buildArgumentsString = "";
  buildConfigurationList = D7D12762170E4CF98A79B5EF /* Build configuration list for PBXLegacyTarget "!UpdateWorkspace" */;
  buildPhases = (
  );
  buildToolPath = /Users/danielcollin/unity_ps3/ps3/Projects/JamGenerated/_workspace.xcode_/updateworkspace;
  dependencies = (
  );
  name = "!UpdateWorkspace";
  passBuildSettingsInEnvironment = 1;
  productName = "!UpdateWorkspace";
  --]]

  p:write('\t\t', newid(name .. "Target"), ' /* ', name, ' */ = {\n')
  p:write('\t\t\tisa = PBXLegacyTarget;\n')
  p:write('\t\t\tbuildArgumentsString = "', project.MetaData.BuildArgs, '";\n') 
  p:write('\t\t\tbuildConfigurationList = ', newid(name .. 'Config'), ' /* Build configuration list for PBXLegacyTarget "',name, '" */;\n')
  p:write('\t\t\tbuildPhases = (\n')
  p:write('\t\t\t);\n');
  p:write('\t\t\tbuildToolPath = ', project.MetaData.BuildTool, ';\n')
  p:write('\t\t\tbuildWorkingDirectory = ', '..', ';\n')

  p:write('\t\t\tdependencies = (\n\t\t\t);\n')
  p:write('\t\t\tname = "', name, '";\n')
  p:write('\t\t\tpassBuildSettingsInEnvironment = 1;\n')
  p:write('\t\t\tproductName = "', name or "", '";\n')
  p:write('\t\t};\n')
end

local function write_native_targets(p, projects)
  p:write('/* Begin PBXNativeTarget section */\n')

  local categories = {
    ["Program"] = "com.apple.product-type.tool",
    ["StaticLibrary"] = "com.apple.product-type.library.static",
    ["SharedLibrary"] = "com.apple.product-type.library.dynamic",
  }

  for _, project in pairs(projects) do
    local decl = project.Decl

    if not project.IsMeta then
      p:write('\t\t', newid(decl.Name .. "Target"), ' /* ', decl.Name, ' */ = {\n')
      p:write('\t\t\tisa = PBXNativeTarget;\n')
      p:write('\t\t\tbuildConfigurationList = ', newid(decl.Name .. 'Config'), ' /* Build configuration list for PBXNativeTarget "',decl.Name, '" */;\n')
      p:write('\t\t\tbuildPhases = (\n')
      p:write('\t\t\t\t', newid(decl.Name .. "ShellScript"), ' /* ShellScript */,\n') 
      p:write('\t\t\t);\n');
      p:write('\t\t\tbuildRules = (\n\t\t\t);\n')
      p:write('\t\t\tdependencies = (\n\t\t\t);\n')
      p:write('\t\t\tname = "', decl.Name, '";\n') 
      p:write('\t\t\tProductName = "', decl.Name, '";\n') 
      p:write('\t\t\tproductReference = ', newid(decl.Name .. "Program"), ' /* ', decl.Name, ' */;\n ')
      p:write('\t\t\tproductType = "', categories[project.Type] or "", '";\n')
      p:write('\t\t};\n')
    end
  end

  p:write('/* End PBXNativeTarget section */\n')
end


local function write_header(p)
  p:write('// !$*UTF8*$!\n')
  p:write('{\n')
  p:write('\tarchiveVersion = 1;\n')
  p:write('\tclasses = {\n')
  p:write('\t};\n')
  p:write('\tobjectVersion = 45;\n')
  p:write('\tobjects = {\n')
  p:write('\n')
end

local function get_projects(raw_nodes, env, hints, ide_script)
  -- Filter out stuff we don't care about.
  local units = util.filter(raw_nodes, function (u)
    return u.Decl.Name and project_types[u.Keyword]
  end)
  
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
  local projects = {}

  for _, unit in ipairs(units) do
    local decl = unit.Decl
    local name = decl.Name

    local sources = {}
    local generated = {}
    for build_id, dag_node in pairs(decl.__DagNodes) do
      get_sources(dag_node, sources, generated, dag_node_lut)
    end

    -- Explicitly add all header files too as they are not picked up from the DAG
    -- Also pick up headers from non-toplevel DAGs we're depending on
    get_headers(unit, sources, dag_node_lut, name_to_dags)

    -- Figure out which project should get this data.
    local output_name = name
    local ide_hints = unit.Decl.IdeGenerationHints
    if ide_hints then
      if ide_hints.OutputProject then
        output_name = ide_hints.OutputProject
      end
    end
	
    -- Rebuild source list with ids that are needed by the xcode project layout
    local source_list = {}
    for src, _ in pairs(sources) do
      local norm_src = path.normalize(src)
--      if not grabbed_sources[norm_src] then
        grabbed_sources[norm_src] = unit
        source_list[newid(norm_src)] = norm_src
--      end
    end
    
    projects[name] = {
      Type = unit.Keyword,
      Decl = decl,
      Sources = source_list,
      RelativeFilename = name,
      Guid = newid(name .. "ProjectId"),
	  IdeGenerationHints = unit.Decl.IdeGenerationHints
    }
  end

  for _, unit in ipairs(raw_nodes) do
    if unit.Keyword == "OsxBundle" then
      local decl = unit.Decl
	  decl.Name = "OsxBundle"

      local source_list = {[newid(decl.InfoPList)] = decl.InfoPList}
      for _, resource in ipairs(decl.Resources) do
        if resource.Decl then
          source_list[newid(resource.Decl.Source)] = resource.Decl.Source
        end
      end

      projects["OsxBundle"] = {
        Type = unit.Keyword,
        Decl = decl,
        Sources = source_list,
        RelativeFilename = "$(OBJECTDIR)/MyApp.app",
        Guid = newid("OsxBundle"),
      }
    end
  end

  return projects
end

local function split(fn)
  local dir, file = fn:match("^(.*)[/\\]([^\\/]*)$")
  if not dir then
    return ".", fn
  else
    return dir, file
  end
end

local function split_str(str, pat)
   local t = {}  -- NOTE: use {n = 0} in Lua-5.0
   local fpat = "(.-)" .. pat
   local last_end = 1
   local s, e, cap = str:find(fpat, 1)
   while s do
      if s ~= 1 or cap ~= "" then
   table.insert(t,cap)
      end
      last_end = e+1
      s, e, cap = str:find(fpat, last_end)
   end
   if last_end <= #str then
      cap = str:sub(last_end)
      table.insert(t, cap)
   end
   return t
end

local function print_children_2(p, groupname, key, children, path)
--  print("folder "..groupname.." ("..path..") "..key)
  for name, c in pairs(children) do
    if c.Type > 0 then
      print_children_2(p, name, c.Key, c.Children, c.Type == 1 and path..'/'..name or path)
	end
  end
  
  p:write('\t\t', key, ' /* ', path, ' */ = {\n')
  p:write('\t\t\tisa = PBXGroup;\n')
  p:write('\t\t\tchildren = (\n')

  local dirs = {}
  local files = {}

  for name, ref in pairs(children) do
    if ref.Type > 0 then
      dirs[#dirs + 1] = { Key = ref.Key, Name = name }
    else
      files[#files + 1] = { Key = ref.Key, Name = name }
    end
  end

  table.sort(dirs, function(a, b)  return a.Name < b.Name end)
  table.sort(files, function(a, b) return a.Name < b.Name end)

  for i, ref in pairs(dirs) do
  p:write(string.format('\t\t\t\t%s /* %s */,\n', ref.Key, path .. '/' .. ref.Name))
  end

  for i, ref in pairs(files) do
  p:write(string.format('\t\t\t\t%s /* %s */,\n', ref.Key, path .. '/' .. ref.Name))
  end

  p:write('\t\t\t);\n')
  p:write('\t\t\tname = "', groupname, '"; \n');
  p:write('\t\t\tsourceTree = "<group>";\n');
  p:write('\t\t};\n')
end

local function prune_groups(group)
  local i = 0
  local first_name
  local first_child
  
  for name, child in pairs(group.Children) do
    first_name = name
    first_child = child
    i = i + 1
  end
  
  if i == 1 and first_child.Type > 0 then
    local new_name = prune_groups(first_child)
    group.Children = first_child.Children;
    if not new_name then
      new_name = first_name
    end
    return new_name
    
  else
    local children = {}
    for name, child in pairs(group.Children) do
      if child.Type > 0 then
        local new_name = prune_groups(child)
        if new_name then
          name = new_name
        end
      end
      children[name] = child
    end
    group.children = children
    return nil
  end
  
end

local function get_group(path, root)
  local parent_group = root
  for i, part in ipairs(split_str(path, "/")) do
    if part ~= '.' then
      local group = parent_group.Children[part]
      if not group then
        group = { Type = 1, Key=newid(util.tostring(parent_group)..part), Children={} }
        parent_group.Children[part] = group
      end
      parent_group = group
    end
  end
  return parent_group
end

local function make_groups(files, key)
  local filelist = sort_filelist(files)
  local group = { Type = 2, Key = key, Children = {} }
  
  for _, entry in pairs(filelist) do
    local parent_group = group
    local path, filename = split(entry.Value)
	for i, part in ipairs(split_str(path, "/")) do
      if part ~= '.' then
        local grp = parent_group.Children[part]
        if grp == nil then
		  grp = { Type = 1, Key=newid(util.tostring(parent_group)..part), Children={} }
          parent_group.Children[part] = grp
        end
        parent_group = grp
      end
    end
    parent_group.Children[filename] = { Type = 0, Key = entry.Key }
  end
  
  -- prune single-entry groups
  prune_groups(group)
  
  return group
end


local function write_groups(p, groups)
  p:write('/* Begin PBXGroup section */\n')

  local all_targets_name = "AllTargets.workspace"
  local all_targets_id = newid(all_targets_name)
  print_children_2(p, all_targets_name, all_targets_id, groups, '.');

  p:write('/* End PBXGroup section */\n\n')
end

local function write_projects(p, projects)

  local all_targets_name = "AllTargets.workspace"
  local all_targets_id = newid(all_targets_name)

  local project_id = newid("ProjectObject")
  local project_config_list_id = newid("ProjectObjectConfigList")

  p:write('/* Begin PBXProject section */\n')
  p:write('\t\t', project_id, ' /* Project object */ = {\n')
  p:write('\t\t\tisa = PBXProject;\n')
  p:write('\t\t\tbuildConfigurationList = ', project_config_list_id, ' /* Build configuration list for PBXProject "', "Project Object", '" */;\n')
  p:write('\t\t\tcompatibilityVersion = "Xcode 3.1";\n')
  p:write('\t\t\thasScannedForEncodings = 1;\n')
  p:write('\t\t\tmainGroup = ', all_targets_id, ' /* ', all_targets_name, ' */;\n')
  p:write('\t\t\tprojectDirPath = "";\n')
  p:write('\t\t\tprojectRoot = "";\n')
  p:write('\t\t\ttargets = (\n')

  for _, project in pairs(projects) do
    p:write(string.format('\t\t\t\t%s /* %s */,\n', newid(project.Decl.Name .. "Target"), project.Decl.Name))
  end

  p:write('\t\t\t);\n')
  p:write('\t\t};\n')
  p:write('/* End PBXProject section */\n')
end

local function write_shellscript(p, project, set_env)
  local name = project.Decl.Name
  p:write('\t\t', newid(name .. "ShellScript"), ' /* ShellScript */ = {\n')
  p:write('\t\t\tisa = PBXShellScriptBuildPhase;\n')
  p:write('\t\t\tbuildActionMask = 2147483647;\n')
  p:write('\t\t\tfiles = (\n')
  p:write('\t\t\t);\n')
  p:write('\t\t\tinputPaths = (\n')
  p:write('\t\t\t);\n')
  p:write('\t\t\toutputPaths = (\n')
  p:write('\t\t\t);\n')
  p:write('\t\t\trunOnlyForDeploymentPostprocessing = 0;\n')
  p:write('\t\t\tshellPath = /bin/sh;\n')

  p:write('\t\t\tshellScript = "')
  for i, var in ipairs(set_env) do
    p:write('export ', var, '=', os.getenv(var), '\n')
  end
  p:write(TundraExePath, ' -C .. -v ${CONFIG}-${VARIANT}-${SUBVARIANT} ${TARGET_NAME}')
  p:write('";\n')
  
  p:write('\t\t};\n')
end

local function write_configs(p, projects, config_tuples, env, set_env)

  p:write('/* Begin XCBuildConfiguration section */\n')

  -- I wonder if we really need to do it this way for all configs?

  for pname, project in pairs(projects) do
    for _, tuple in ipairs(config_tuples) do
      local is_macosx_native = false

      for _, host in util.nil_ipairs(tuple.Config.SupportedHosts) do
        if host == "macosx" then
          is_macosx_native = true
        end
      end

      if "macosx" == tuple.Config.DefaultOnHost then
        is_macosx_native = true
      end

      local config_id = newid(project.Decl.Name .. tuple.XcodeConfig)

      p:write('\t\t', config_id, ' = {\n')
      p:write('\t\t\tisa = XCBuildConfiguration;\n')

      -- Don't add any think extra if subvariant is default

      p:write('\t\t\tbuildSettings = {\n')

      if is_macosx_native then 
        p:write('\t\t\t\tARCHS = "$(NATIVE_ARCH_ACTUAL)";\n') 
      end

      p:write('\t\t\t\tVARIANT = "', tuple.Variant.Name, '";\n') 
      p:write('\t\t\t\tCONFIG = "', tuple.Config.Name, '";\n')
      p:write('\t\t\t\tSUBVARIANT = "', tuple.SubVariant, '";\n')

      if is_macosx_native and not project.IsMeta then 
        p:write('\t\t\t\tCONFIGURATION_BUILD_DIR = "Built/', tuple.XcodeConfig, '";\n')
      end

      p:write('\t\t\t\tPRODUCT_NAME = "$(TARGET_NAME)";\n')
	  
--	  for i, var in ipairs(set_env) do
--      p:write('\t\t\t\t', var, ' = "', os.getenv(var), '";\n')
--	  end

      p:write('\t\t\t};\n')
      p:write('\t\t\tname = "',tuple.XcodeConfig , '";\n')
      p:write('\t\t};\n')
    end
  end

  -- PBXProject configurations

  for _, tuple in ipairs(config_tuples) do
    local config_id = newid("ProjectObject" .. tuple.XcodeConfig)

    p:write('\t\t', config_id, ' = {\n')
    p:write('\t\t\tisa = XCBuildConfiguration;\n')

    p:write('\t\t\tbuildSettings = {\n')
    
    p:write('\t\t\t\tVARIANT = "', tuple.Variant.Name, '";\n')
    p:write('\t\t\t\tCONFIG = "', tuple.Config.Name, '";\n')
    p:write('\t\t\t\tSUBVARIANT = "', tuple.SubVariant, '";\n')

    for i, var in ipairs(set_env) do
      p:write('\t\t\t\t', var, ' = "', os.getenv(var), '";\n')
    end
    
    p:write('\t\t\t};\n')
    p:write('\t\t\tname = "',tuple.XcodeConfig , '";\n')
    p:write('\t\t};\n')
  end

  p:write('/* End XCBuildConfiguration section */\n')

end

local function write_config_lists(p, projects, config_tuples)
  p:write('/* Begin XCConfigurationList section */\n')

  local default_config = "";

  -- find the default config

  for _, tuple in ipairs(config_tuples) do
    local is_macosx_native = tuple.Config.Name:match('^(%macosx)%-')
    if is_macosx_native and tuple.Variant.Name == "debug" then
      default_config = tuple.XcodeConfig
      break
    end
  end

  -- if we did't find a default config just grab the first one

  if default_config == "" then
    default_config = config_tuples[1].XcodeConfig
  end

  for pname, project in pairs(projects) do
    local config_id = newid(project.Decl.Name .. 'Config')

    p:write('\t\t', config_id, ' /* Build config list for "', project.Decl.Name, '" */ = {\n')
    p:write('\t\t\tisa = XCConfigurationList;\n')

    -- Don't add any think extra if subvariant is default

    p:write('\t\t\tbuildConfigurations = (\n')
  
    for _, tuple in ipairs(config_tuples) do
      p:write(string.format('\t\t\t\t%s /* %s */,\n', newid(project.Decl.Name .. tuple.XcodeConfig), tuple.XcodeConfig))
    end

    p:write('\t\t\t);\n')
    p:write('\t\t\tdefaultConfigurationIsVisible = 1;\n')
    p:write('\t\t\tdefaultConfigurationName = "', default_config, '";\n')

    p:write('\t\t};\n')
  end
  
  -- PBXProject configuration list

  local config_id = newid("ProjectObjectConfigList")

  p:write('\t\t', config_id, ' /* Build config list for PBXProject */ = {\n')
  p:write('\t\t\tisa = XCConfigurationList;\n')

  -- Don't add any think extra if subvariant is default

  p:write('\t\t\tbuildConfigurations = (\n')
  
  for _, tuple in ipairs(config_tuples) do
    p:write(string.format('\t\t\t\t%s /* %s */,\n', newid("ProjectObject" .. tuple.XcodeConfig), tuple.XcodeConfig))
  end

  p:write('\t\t\t);\n')
  p:write('\t\t\tdefaultConfigurationIsVisible = 1;\n')
  p:write('\t\t\tdefaultConfigurationName = "', default_config, '";\n')
  p:write('\t\t};\n')

  p:write('/* End XCConfigurationList section */\n')

end

local function write_footer(p)
  p:write('\t};\n')
  p:write('\trootObject = ', newid("ProjectObject"), ' /* Project object */;\n')
  p:write('}\n')
end

function xcode_generator:generate_files(ngen, config_tuples, raw_nodes, env, default_names, hints, ide_script)
  print("Generating project files for xcode 5")
  assert(config_tuples and #config_tuples > 0)

  local msvc_hints  = (hints and hints.Msvc) or {}
  local xcode_hints = (hints and hints.Xcode) or {}
  
  local variant_mappings = msvc_hints.VariantMappings or {}
  for _, tuple in ipairs(config_tuples) do
    tuple.XcodeConfig = variant_mappings[tuple.Variant.Name] or tuple.Variant.Name
  end
 
  local base_dir = xcode_hints.BaseDir and (xcode_hints.BaseDir .. '/') or env:interpolate('$(OBJECTROOT)$(SEP)')
  native.mkdir(base_dir)

  local projects = get_projects(raw_nodes, env, xcode_hints, ide_script)
  
  local solution_hints = xcode_hints.Projects
  if not solution_hints then
    print("No IdeGenerationHints.Xcode.Projects specified - using defaults")
    solution_hints = {
      ['tundra-generated.sln'] = {}
    }
  end
  
  local source_list = {
	[newid("tundra.lua")] = "tundra.lua"
  }
  local units = io.open("units.lua")
  if units then
    source_list[newid("units.lua")] = "units.lua"
	io.close(units)
  end

  local meta_name = "!BuildWorkspace"
  local build_project = {
    Decl = { Name = meta_name, },
    Type = "LegacyTarget",
    RelativeFilename = "",
    Sources = source_list,
    Guid = newid(meta_name .. 'ProjectId'),
    IsMeta = true,
    MetaData = { BuildArgs = "-v $(CONFIG)-$(VARIANT)-$(SUBVARIANT)", BuildTool = TundraExePath },
  }
  local meta_name = "!UpdateWorkspace"
  local generate_project = {
    Decl = { Name = meta_name, },
    Type = "LegacyTarget", 
    RelativeFilename = "",
    Sources = source_list, 
    Guid = newid(meta_name .. 'ProjectId'),
    IsMeta = true,
    MetaData = { BuildArgs = "-g " .. ide_script, BuildTool = TundraExePath },
  }

  for name, data in pairs(solution_hints) do
    print("Generating xcode project "..name)
    local sln_projects = {
      ["!BuildWorkspace"] = build_project,
      ["!UpdateWorkspace"]= generate_project
    }
	
    if data.Projects then
      for _, pname in ipairs(data.Projects) do
        local project = projects[pname]
        if not project then
          errorf("can't find project %s for inclusion in %s -- check your Projects data", pname, name)
        end
        --print("  incorporating project "..pname)
        sln_projects[pname] = project
      end
    else
      -- all the projects (that are not meta)
	  for pname, project in ipairs(projects) do
        print("  incorporating project "..pname)
		sln_projects[pname] = project
	  end
    end
    
    -- build the source list
    source_list = {}
    local groups = {}
    
    for pname, project in pairs(sln_projects) do
      --print("  add files for project "..pname)
      
      local hints       = project.IdeGenerationHints
      local msvc_hints  = hints and hints.Msvc
      local fname       = (msvc_hints and msvc_hints.SolutionFolder) or "<root>"
      local group       = groups[fname]
      if not group then
        group = { Type = 2, Key = newid("Folder"..fname), Children = {} }
        groups[fname] = group
      end
      
      local project_root = { Type = 2, Key = project.Guid, Children = {} }
      group.Children[pname] = project_root

      for key, fn in pairs(project.Sources) do
        --print("    " .. fn)
        source_list[key] = fn
        local path, name = split(fn)
        local group = get_group(path, project_root)
        group.Children[name] = { Type = 0, Key = key }
      end
      
      -- prune single-entry groups
      prune_groups(project_root)

      -- include executable names in the source list as well
      if project.Type == "Program" then
        source_list[newid(project.Decl.Name .. "Program")] = project.Decl.Name
      end
    end
    
    local root = groups["<root>"];
    for name, group in pairs(groups) do
      if group ~= root then
        root.Children[name] = group
      end
    end


    --Write out project
	local proj_dir	= base_dir .. path.drop_suffix(name) .. ".xcodeproj/"
	native.mkdir(proj_dir)
	local p = io.open(path.join(proj_dir, "project.pbxproj"), 'wb')
    
    local set_env = xcode_hints.EnvVars or {}

	write_header(p)
	write_file_refs(p, source_list)
    write_groups(p, root.Children)

    p:write('/* Begin PBXLegacyTarget section */\n')
	write_legacy_target(p, build_project, env)
	write_legacy_target(p, generate_project, env)
    p:write('/* End PBXLegacyTarget section */\n')

	write_native_targets(p, sln_projects)
	write_projects(p, sln_projects)
    
    p:write('/* Begin PBXShellScriptBuildPhase section */\n')
    for _, project in pairs(projects) do
      if not project.IsMeta then
        write_shellscript(p, project, set_env)
      end
    end
    p:write('/* End PBXShellScriptBuildPhase section */\n')

	write_configs(p, sln_projects, config_tuples, env, set_env)
	write_config_lists(p, sln_projects, config_tuples)
	write_footer(p)
  end
end

nodegen.set_ide_backend(function(...)
  local state = setmetatable({}, xcode_generator)
  state:generate_files(...)
end)

