module(..., package.seeall)

local util = require "tundra.util"
local nodegen = require "tundra.nodegen"
local native = require "tundra.native"

local project_types = util.make_lookup_table {
	"Program", "SharedLibrary", "StaticLibrary", "CSharpExe", "CSharpLib"
}

function extract_data(unit, env, proj_extension)
	local decl = unit.Decl

	if decl.Name and project_types[unit.Keyword] then

		local relative_fn = decl.Name .. proj_extension
		local sources = nodegen.flatten_list("*-*-*-*", decl.Sources) or {}
		sources = util.filter(sources, function (x) return type(x) == "string" end)

		if decl.SourceDir then
			sources = util.map(sources, function (x) return decl.SourceDir .. x end)
		end

		sources = util.map(sources, function (x) return x:gsub('/', '\\') end)

		return {
			Type = unit.Keyword,
			Decl = decl,
			Sources = sources,
			RelativeFilename = relative_fn,
			Filename = env:interpolate("$(OBJECTROOT)$(SEP)" .. relative_fn),
			Guid = native.digest_guid(decl.Name),
		}
	else
		return nil
	end
end

