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

local _tostring = tostring
module(..., package.seeall)

function tostring(value)
	local str = ''

	if (type(value) ~= 'table') then
		if (type(value) == 'string') then
			str = string.format("%q", value)
		else
			str = _tostring(value)
		end
	else
		local auxTable = {}
		for k, v in pairs(value) do
			auxTable[#auxTable + 1] = k
		end
		table.sort(auxTable, function (a, b) return _tostring(a) < _tostring(b) end)

		str = str..'{'
		local separator = ""
		local entry = ""
		for index, fieldName in ipairs(auxTable) do
			if ((tonumber(fieldName)) and (tonumber(fieldName) > 0)) then
				entry = tostring(value[tonumber(fieldName)])
			else
				entry = tostring(fieldName) .. " = " .. tostring(rawget(value, fieldName))
			end
			str = str..separator..entry
			separator = ", "
		end
		str = str..'}'
	end
	return str
end

function map(t, fn)
	local result = {}
	for idx = 1, #t do
		result[idx] = fn(t[idx])
	end
	return result
end

function mapnil(table, fn)
	if not table then
		return nil
	else
		return map(table, fn)
	end
end

function get_named_arg(tab, name, context)
	local v = tab[name]
	if v then
		return v
	else
		if context then
			error(context .. ": argument " .. name .. " must be specified", 3)
		else
			error("argument " .. name .. " must be specified", 3)
		end
	end
end

function parse_cmdline(args, blueprint)
	local index, max = 2, #args
	local options, targets = {}, {}
	local lookup = {}

	for _, opt in ipairs(blueprint) do
		if opt.Short then
			lookup[opt.Short] = opt
		end
		if opt.Long then
			lookup[opt.Long] = opt
		end
	end

	while index <= max do
		local s = args[index]
		local key, val

		if s:sub(1, 2) == '--' then
			key, val = s:match("^%-%-([-a-zA-Z0-9]+)=(.*)$")
			if not key then
				key = s:sub(3)
			end
		elseif s:sub(1, 1) == '-' then
			key = s:sub(2,2)
			if s:len() > 2 then
				val = s:sub(3)
			end
		else
			table.insert(targets, s)
		end

		if key then
			local opt = lookup[key]
			if not opt then
				return nil, nil, "Unknown option " .. s
			end
			if opt.HasValue then
				if not val then
					index = index + 1
					val = args[index]
				end
				if val then
					options[opt.Name] = val
				else
					return nil, nil, "Missing value for option "..s
				end
			else
				local v = options[opt.Name] or 0
				options[opt.Name] = v + 1
			end
		end

		index = index + 1
	end

	return options, targets
end

function clone_table(t)
	if t then
		local r = {}
		for k, v in pairs(t) do
			r[k] = v
		end
		return r
	else
		return nil
	end
end

function clone_array(t)
	local r = {}
	for k, v in ipairs(t) do
		r[k] = v
	end
	return r
end

function merge_arrays(...)
	local result = {}
	local count = select('#', ...)
	for i = 1, count do
		local tab = select(i, ...)
		if tab then
			for _, v in ipairs(tab) do
				result[#result + 1] = v
			end
		end
	end
	return result
end

function merge_arrays_2(a, b)
	if a and b then
		return merge_arrays(a, b)
	elseif a then
		return a
	elseif b then
		return b
	else
		return {}
	end
end

function matches_any(str, patterns)
	for _, pattern in ipairs(patterns) do
		if str:match(pattern) then
			return true
		end
	end
	return false
end

function return_nil()
end

function nil_pairs(t)
	if t then
		return next, t
	else
		return return_nil
	end
end

function nil_ipairs(t)
	if t then
		return ipairs(t)
	else
		return return_nil
	end
end

function clear_table(tab)
	local key, val = next(tab)
	while key do
		tab[key] = nil
		key, val = next(tab, key)
	end
	return tab
end

function filter(tab, predicate)
	local result = {}
	for _, x in ipairs(tab) do
		if predicate(x) then
			result[#result + 1] = x
		end
	end
	return result
end

function filter_in_place(tab, predicate)
	local i, limit = 1, #tab
	while i <= limit do
		if not predicate(tab[i]) then
			table.remove(tab, i)
			limit = limit - 1
		else
			i = i + 1
		end
	end
	return tab
end

function append_table(result, items)
	local offset = #result
	for i = 1, #items do
		result[offset + i] = items[i]
	end
	return result
end

function flatten(array)
	local function iter(item, accum)
		if type(item) == 'table' then
			for _, sub_item in ipairs(item) do
				iter(sub_item, accum)
			end
		else
			accum[#accum + 1] = item
		end
	end
	local accum = {}
	iter(array, accum)
	return accum
end

function memoize(closure)
	local result = nil
	return function(...)
		if not result then
			result = assert(closure(...))
		end
		return result
	end
end

function uniq(array)
	local seen = {}
	local result = {}
	for _, val in ipairs(array) do
		if not seen[val] then
			seen[val] = true
			result[#result + 1] = val
		end
	end
	return result
end

function make_lookup_table(array)
	local result = {}
	for _, item in nil_ipairs(array) do
		result[item] = true
	end
	return result
end
