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

module(..., package.seeall)

local native = require "tundra.native"

function split(fn)
	local dir, file = fn:match("^(.*)[/\\]([^\\/]*)$")
	if not dir then
		return ".", fn
	else
		return dir, file
	end
end

normalize = native.sanitize_path

function join(dir, fn)
	return normalize(dir .. '/' .. fn)
end

function get_filename_dir(fn)
	return select(1, split(fn))
end

function get_filename(fn)
	return select(2, split(fn))
end

function get_extension(fn)
	return fn:match("(%.[^.]+)$") or ""
end

function drop_suffix(fn)
	assert(type(fn) == "string")
	return fn:match("^(.*)%.[^./\\]+$") or fn
end

function get_filename_base(fn)
	assert(fn, "nil filename")
	local _,_,stem = fn:find("([^/\\]+)%.[^.]*$")
	if stem then return stem end
	_,_,stem = fn:find("([^/\\]+)$")
	return stem
end

function make_object_filename(env, src_fn, suffix)
	local object_fn

	local src_suffix = get_extension(src_fn):sub(2)

	-- Drop leading $(OBJECTDIR)[/\\] in the input filename.
	do
		local pname = src_fn:match("^%$%(OBJECTDIR%)[/\\](.*)$")
		if pname then
			object_fn = pname
		else
			object_fn = src_fn
		end
	end

	-- Compute path under OBJECTDIR we want for the resulting object file.
	-- Replace ".." with "dotdot" to avoid creating files outside the
	-- object directory. Also salt the generated object name with the source
	-- suffix, so that multiple source files with the same base name don't end
	-- up clobbering each other (Tundra emits an error for this when checking
	-- the DAG)
	do
		local relative_name = drop_suffix(object_fn:gsub("%.%.", "dotdot"))
		object_fn = "$(OBJECTDIR)/$(UNIT_PREFIX)/" .. relative_name .. "__" .. src_suffix .. suffix
	end

	return object_fn
end

function is_absolute(v)
	local first = v:sub(1, 1)
	if first == "/" or first == "\\" then
		return true
	end

	if native.host_platform == "windows" then
		local sep = v:sub(2, 3)
		if sep == ':\\' or sep == ':/' then
			return true
		end
	end

	return false
end
