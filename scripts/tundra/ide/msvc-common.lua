module(..., package.seeall)

local util = require "tundra.util"
local nodegen = require "tundra.nodegen"
local native = require "tundra.native"
local path = require "tundra.path"

local project_types = util.make_lookup_table {
  "Program", "SharedLibrary", "StaticLibrary", "CSharpExe", "CSharpLib", "ObjGroup",
}

local toplevel_stuff = util.make_lookup_table {
  ".exe", ".lib", ".dll",
}

local binary_extension = util.make_lookup_table {
  ".exe", ".lib", ".dll", ".pdb", ".res", ".obj"
}

local header_exts = util.make_lookup_table {
  ".h", ".hpp", ".hh", ".inl",
}

-- Scan for sources, following dependencies until those dependencies seem to be
-- a different top-level unit
local function get_sources(dag, sources, generated, level, dag_lut)
  for _, output in util.nil_ipairs(dag.outputs) do
    local ext = path.get_extension(output)
    if toplevel_stuff[ext] then
      -- Terminate here, something else will want the sources files from this sub-DAG
      return
    end
    if not binary_extension[ext] then
      generated[output] = true
      sources[output] = true -- pick up generated headers
    end
  end

  for _, input in util.nil_ipairs(dag.inputs) do
    local ext = path.get_extension(input)
    if not binary_extension[ext] then
      sources[input] = true
    end
  end

  for _, dep in util.nil_ipairs(dag.deps) do
    if not dag_lut[dep] then -- don't go into other top-level DAGs
      get_sources(dep, sources, generated, level + 1, dag_lut)
    end
  end
end

function get_guid_string(data)
  local sha1 = native.digest_guid(data)
  local guid = sha1:sub(1, 8) .. '-' .. sha1:sub(9,12) .. '-' .. sha1:sub(13,16) .. '-' .. sha1:sub(17,20) .. '-' .. sha1:sub(21, 32)
  assert(#guid == 36) 
  return guid:upper()
end

local function get_headers(unit, source_lut, dag_lut)
  local src_dir = ''
  if unit.Decl.SourceDir then
    src_dir = unit.Decl.SourceDir .. '/'
  end
  for _, src in util.nil_ipairs(nodegen.flatten_list('*-*-*-*', unit.Decl.Sources)) do
    if type(src) == "string" then
      local ext = path.get_extension(src)
      if header_exts[ext] then
        local full_path = path.normalize(src_dir .. src)
        source_lut[full_path] = true
      end
    end
  end

  local function toplevel(u)
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
      get_headers(dep, source_lut, dag_lut)
    end
  end
end

function make_project_data(units, env, proj_extension, hints)

  local base_dir = hints.MsvcSolutionDir and (hints.MsvcSolutionDir .. '\\') or env:interpolate('$(OBJECTROOT)$(SEP)')
  native.mkdir(base_dir)

  local project_by_name = {}
  local all_sources = {}
  local dag_node_lut = {} -- lookup table of all named, top-level DAG nodes 

  -- Map out all top-level DAG nodes
  for _, unit in ipairs(units) do
    local decl = unit.Decl

    if decl.Name and project_types[unit.Keyword] then
      local dag_nodes = assert(decl.__DagNodes, "no dag nodes for " .. decl.Name)
      for build_id, dag_node in pairs(dag_nodes) do
        dag_node_lut[dag_node] = unit
      end
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
        Guid             = get_guid_string(name),
      }
    end
    return project_by_name[name]
  end

  for _, unit in ipairs(units) do
    local decl = unit.Decl
    local name = decl.Name

    if name and project_types[unit.Keyword] then

      local source_lut = {}
      local generated_lut = {}
      for build_id, dag_node in pairs(decl.__DagNodes) do
        get_sources(dag_node, source_lut, generated_lut, 0, dag_node_lut)
      end

      -- Explicitly add all header files too as they are not picked up from the DAG
      -- Also pick up headers from non-toplevel DAGs we're depending on
      get_headers(unit, source_lut, dag_node_lut)

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
        proj.DagNodes           = unit.__DagNodes
      end

      local cwd = native.getcwd()
      for src, _ in pairs(source_lut) do
        local is_generated = generated_lut[src]
        proj.Sources[#proj.Sources+1] = {
          Path        = src:gsub('/', '\\'),
          Generated   = is_generated,
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

  local solution_hints = hints.MsvcSolutions
  if not solution_hints then
    print("No IdeGenerationHints.MsvcSolutions specified - using defaults")
    solution_hints = {
      ['tundra-generated.sln'] = {}
    }
  end

  local projects = util.table_values(project_by_name)

  local solutions = {}

  for name, data in pairs(solution_hints) do
    local sln_projects
    if data.Projects then
      sln_projects = {}
      for _, pname in ipairs(data.Projects) do
        local pp = project_by_name[pname]
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
      Name               = meta_name,
      IdeGenerationHints = { Msvc = { SolutionFolder = "Build System Meta" } },
      Type               = "meta",
      RelativeFilename   = meta_name .. ".vcxproj",
      Filename           = base_dir .. meta_name .. ".vcxproj",
      Sources            = source_list,
      Guid               = get_guid_string(meta_name),
      IsMeta             = true,
    }

    sln_projects[#sln_projects + 1] = meta_proj
    projects[#projects + 1] = meta_proj

    solutions[#solutions + 1] = {
      Filename = base_dir .. name,
      Projects = sln_projects
    }
  end

  return solutions, projects
end

