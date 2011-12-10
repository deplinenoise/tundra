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

-- files.lua - File management utilities, e.g. copying

module(..., package.seeall)

local decl = require "tundra.decl"
local nodegen = require "tundra.nodegen"

function copy_file(env, source, target, pass, deps)
	return env:make_node {
		Label = "CopyFile $(@)",
		Action = "$(_COPY_FILE)",
		Pass = pass,
		InputFiles = { source },
		OutputFiles = { target },
		Dependencies = deps,
	}
end

function hardlink_file(env, source, target, pass, deps)
	return env:make_node {
		Label = "HardLink $(@)",
		Action = "$(_HARDLINK_FILE)",
		Pass = pass,
		InputFiles = { source },
		OutputFiles = { target },
		Dependencies = deps,
	}
end

local _copy_meta = { }

function _copy_meta:create_dag(env, data, deps)
	return copy_file(env, data.Source, data.Target, data.Pass, deps)
end

local _hardlink_meta = { }

function _hardlink_meta:create_dag(env, data, deps)
	return hardlink_file(env, data.Source, data.Target, data.Pass, deps)
end

local blueprint = {
	Source = {
		Importance = "required",
		Help = "Specify source filename",
		Type = "string",
	},
	Target = {
		Importance = "required",
		Help = "Specify target filename",
		Type = "string",
	},
}

nodegen.add_evaluator("CopyFile", _copy_meta, blueprint)
nodegen.add_evaluator("HardLinkFile", _hardlink_meta, blueprint)


