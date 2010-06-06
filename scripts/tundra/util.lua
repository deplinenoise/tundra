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

function GetNamedArg(tab, name, context)
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

function ParseCommandline(args, blueprint)
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

function SerializeTable(stream, tab)
	stream:write("{\n")
	for k, v in pairs(tab) do
		stream:write("[")
		SerializeExpr(stream, k)
		stream:write("] = ")
		SerializeExpr(stream, v)
		stream:write(",")
	end
	stream:write("}\n")
end

function SerializeString(stream, s)
	stream:write(string.format("%q", s))
end

function SerializeExpr(stream, d)
	local t = type(d)
	if not d then
		stream:write("nil")
	elseif t == "table" then
		SerializeTable(stream, d)
	elseif t == "string" then
		SerializeString(stream, d)
	elseif t == "number" then
		stream:write(tostring(d))
	else
		error("can't serialize type " .. t)
	end
end

function Serialize(stream, d)
	stream:write('return ')
	SerializeExpr(stream, d)
end

local serialize_num_key = {}

function SerializeCycle(stream, name, d, state, is_local)
	print('SerializeCycle', name, _tostring(d))

	local function basicSerialize(o)
		if type(o) == "number" then
			return _tostring(o)
		elseif type(o) == "boolean" then
			return o and "true" or "false"
		elseif type(o) == "string" then
			return string.format("%q", o)
		elseif type(o) == "table" then
			if state[o] then
				return state[o]
			else
				local n = state[serialize_num_key] or 1
				state[serialize_num_key] = n + 1
				local k = 'temp' .. n
				SerializeCycle(stream, k, o, state, true)
				return k
			end
		else
			error("can't serialize " .. type(o))
		end
	end

	state = state or {}
	if is_local then
		stream:write('local ')
	end

	if type(d) == "number" or type(d) == "string" or type(d) == "boolean" then
		stream:write(basicSerialize(d, stream), "\n")
	elseif type(d) == "table" then
		local serialize_fn = d.Serialize
		if type(serialize_fn) == "function" then
			local expr = serialize_fn(d, stream, state)
			stream:write(name, " = ", expr, "\n")
		else
			stream:write(name, " = ")

			if state[d] then
				stream:write(state[d], "\n")
			else
				state[d] = name
				stream:write("{}\n")
				for k, v in pairs(d) do
					k = basicSerialize(k, stream)
					local fname = string.format("%s[%s]", name, k)
					SerializeCycle(stream, fname, v, state)
				end
			end
		end
	else
		error("can't serialize " .. type(d))
	end
end

function CloneTable(t)
	local r = {}
	for k, v in pairs(t) do
		r[k] = v
	end
	return r
end

function CloneArray(t)
	local r = {}
	for k, v in ipairs(t) do
		r[k] = v
	end
	return r
end

function MergeArrays(...)
	local result = {}
	for _, t in ipairs({...}) do
		for _, v in ipairs(t) do
			result[#result + 1] = v
		end
	end
	return result
end

function MatchesAny(str, patterns)
	for _, pattern in ipairs(patterns) do
		if str:match(pattern) then
			return true
		end
	end
	return false
end

function ReturnNil()
end

function NilPairs(t)
	if t then
		return next, t
	else
		return ReturnNil
	end
end

function NilIPairs(t)
	if t then
		return ipairs(t)
	else
		return ReturnNil
	end
end

function ClearTable(tab)
	local key, val = next(tab)
	while key do
		tab[key] = nil
		key, val = next(tab, key)
	end
	return tab
end

function FilterInPlace(tab, predicate)
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
