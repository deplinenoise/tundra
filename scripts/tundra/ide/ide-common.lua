module(..., package.seeall)

local native  = require "tundra.native"
local nodegen = require "tundra.nodegen"
local path    = require "tundra.path"
local util    = require "tundra.util"


project_types = util.make_lookup_table {
  "Program", "SharedLibrary", "StaticLibrary", "CSharpExe", "CSharpLib", "ObjGroup",
}

binary_extension = util.make_lookup_table {
  ".exe", ".lib", ".dll", ".pdb", ".res", ".obj", ".o", ".a", "",
}

header_exts = util.make_lookup_table {
  ".h", ".hpp", ".hh", ".inl",
}


-- Scan for sources, following dependencies until those dependencies seem to be
-- a different top-level unit
function get_sources(dag, sources, generated, level, dag_lut)
  for _, output in util.nil_ipairs(dag.outputs) do
    local ext = path.get_extension(output)
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


function get_headers(unit, source_lut, dag_lut, name_to_dags)
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
        source_lut[full_path] = true
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
      get_headers(dep, source_lut, dag_lut)
    end
  end
end


local function tundra_cmdline(args)
  local root_dir = native.getcwd()
  return "\"" .. TundraExePath .. "\" -C \"" .. root_dir .. "\" " .. args
end


function project_regen_commandline(ide_script)
  return tundra_cmdline("-g " .. ide_script)
end


function slurp_file(fn)
  local fh, err = io.open(fn, 'rb')
  if fh then
    local data = fh:read("*all")
    fh:close()
    return data
  end
  return ''
end


function replace_if_changed(new_fn, old_fn)
  local old_data = slurp_file(old_fn)
  local new_data = slurp_file(new_fn)
  if old_data == new_data then
    os.remove(new_fn)
    return
  end
  printf("Updating %s", old_fn)
  os.remove(old_fn)
  os.rename(new_fn, old_fn)
end


function find_dag_node_for_config(project, tuple)
  local build_id = string.format("%s-%s-%s", tuple.Config.Name, tuple.Variant.Name, tuple.SubVariant)
  local nodes = project.DagNodes
  if not nodes then
    return nil
  end

  if nodes[build_id] then
    return nodes[build_id]
  end
  errorf("couldn't find config %s for project %s (%d dag nodes) - available: %s",
    build_id, project.Name, #nodes, table.concat(util.table_keys(nodes), ", "))
end
