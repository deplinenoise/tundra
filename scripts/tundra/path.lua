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

function split(fn)
	local dir, file = fn:match("^(.*)[/\\]([^\\/]*)$")
	if not dir then
		return ".", fn
	else
		return dir, file
	end
end

function normalize(fn)
	local pieces = {}
	for piece in string.gmatch(fn, "[^/\\]+") do
		if piece == ".." then
			table.remove(pieces)
		elseif piece == "." then
			-- nop
		else
			pieces[#pieces + 1] = piece
		end
	end
	return table.concat(pieces, '/')
end

function join(dir, fn)
	return normalize(dir .. '/' .. fn)
end

function get_filename_dir(fn)
	return select(2, split(fn))
end

function get_extension(fn)
	return fn:match("(%.[^.]+)$") or ""
end

function drop_suffix(fn)
	return fn:match("^(.*)%.[^./\\]+$") or fn
end

function get_filename_base(fn)
	assert(fn, "nil filename")
	local _,_,stem = fn:find("([^/\\]+)%.[^.]*$")
	if stem then return stem end
	_,_,stem = fn:find("([^/\\]+)$")
	return stem
end

