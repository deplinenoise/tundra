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
		table.foreach(value, function(i, v)
			if (tonumber(i) ~= i) then
				table.insert(auxTable, i)
			else
				table.insert(auxTable, tostring(i))
			end
		end)
		table.sort(auxTable)

		str = str..'{'
		local separator = ""
		local entry = ""
		table.foreachi (auxTable, function (i, fieldName)
			if ((tonumber(fieldName)) and (tonumber(fieldName) > 0)) then
				entry = tostring(value[tonumber(fieldName)])
			else
				entry = fieldName.." = "..tostring(rawget(value, fieldName))
			end
			str = str..separator..entry
			separator = ", "
		end)
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
		local key = nil
		if s:sub(1, 2) == '--' then
			key = s:sub(3)
		elseif s:sub(1, 1) == '-' then
			key = s:sub(2)
		else
			table.insert(targets, s)
		end

		if key then
			local opt = lookup[key]
			if not opt then
				return nil, nil, "Unknown option " .. s
			end
			if opt.HasValue then
				local val = args[index+1]
				if val then
					options[opt.Name] = val
					index = index + 1
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
	local r = {}
	for k, v in pairs(t) do
		r[k] = v
	end
	return r
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
	local count = #...
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
