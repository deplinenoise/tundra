local _outer_env = ...
local depgraph = require("tundra.depgraph")
local util = require("tundra.util")
local path = require("tundra.path")
local native = require("tundra.native")

local function get_cpp_scanner(env, fn)
	local function new_scanner()
		local paths = util.map(env:get_list("CPPPATH"), function (v) return env:interpolate(v) end)
		return depgraph.current_engine:make_cpp_scanner(paths)
	end
	return env:memoize("CPPPATH", "_cpp_scanner", new_scanner)
end

do
	local _anyc_compile = function(env, pass, fn, label, action)
		local function obj_fn()
			if fn:match('^%$%(OBJECTDIR%)') then
				return path.drop_suffix(fn) .. '$(OBJECTSUFFIX)'
			else
				return '$(OBJECTDIR)/' .. path.drop_suffix(fn) .. '$(OBJECTSUFFIX)'
			end
		end
		local object_fn = obj_fn(fn)
		return env:make_node {
			Label = 'Cc $(@)',
			Pass = pass,
			Action = "$(CCCOM)",
			InputFiles = { fn },
			OutputFiles = { object_fn },
			Scanner = get_cpp_scanner(env, fn),
		}
	end

	local cc_compile = function(env, pass, fn)
		return _anyc_compile(env, pass, fn, "Cc $(@)", "$(CCCOM)")
	end

	local cxx_compile = function(env, pass, fn)
		return _anyc_compile(env, pass, fn, "C++ $(@)", "$(CXXCOM)")
	end

	_outer_env:register_implicit_make_fn("c", cc_compile)
	_outer_env:register_implicit_make_fn("cpp", cxx_compile)
	_outer_env:register_implicit_make_fn("cc", cxx_compile)
	_outer_env:register_implicit_make_fn("cxx", cxx_compile)
end

_outer_env:set_many {
	["HEADERS_EXTS"] = { ".h", ".hpp", ".hh", ".hxx" },
	["CPPDEFS"] = "",
	["CCOPTS_DEBUG"] = "",
	["CCOPTS_PRODUCTION"] = "",
	["CCOPTS_RELEASE"] = "",
}
