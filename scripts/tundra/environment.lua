module(..., package.seeall)

local util = require('tundra.util')
local depgraph = require('tundra.depgraph')

--[==[

The environment is a holder for variables and their associated values. Values
are always kept as tables, even if there is only a single value.

FOO = { a b c }

e:Interpolate("$(FOO)") -> "a b c"
e:Interpolate("$(FOO:j=, )") -> "a, b, c"
e:Interpolate("$(FOO:p=-I)") -> "-Ia -Ib -Ic"

Missing keys trigger errors unless a default value is specified.

]==]--

local Environment = {
	SequenceNo = 1,
}

local function NextSequenceNo()
	local sno = Environment.SequenceNo
	Environment.SequenceNo = sno + 1
	return sno
end

function Environment:Create(parent, assignments, obj)
	obj = obj or {}
	setmetatable(obj, self)
	self.__index = self

	obj.vars = {}
	obj.parent = parent
	obj.id = 'env' .. NextSequenceNo() -- For graph dumping purposes

	-- Set up the table of make functions
	obj.MakeFunctions = {} 
	obj.Make = setmetatable({}, {
		__index = function(table, key) return obj:_lookup_make(key) end,
		__newindex = function(table, key, value) obj:RegisterMakeFn(key, value) end
	})

	-- Assign initial bindings
	if assignments then
		obj:SetMany(assignments)
	end

	return obj
end

function Environment:Clone(assignments)
	return Environment:Create(self, assignments)
end

function Environment:_lookup_make(name, real_env)
	real_env = real_env or self
	local entry = self.MakeFunctions[name]
	if entry then
		assert(entry.Name == name)
		local fn = function(args)
			return entry.Function(real_env, args)
		end
		return fn
	elseif self.parent then
		return self.parent:_lookup_make(name, real_env)
	else
		error("'" .. name .. "': no such make function")
	end
end

function Environment:RegisterMakeFn(name, fn, docstring)
	assert(type(name) == "string")
	assert(type(fn) == "function")
	self.MakeFunctions[name] = {
		Name = name,
		Function = fn,
		Doc = docstring or ""
	}
end

function Environment:HasKey(key)
	return self.vars[key] and true or false
end

function Environment:GetVars()
	return self.vars
end

function Environment:SetMany(table)
	for k, v in pairs(table) do
		self:Set(k, v)
	end
end

function Environment:Append(key, value)
	local t = Environment:GetList(key, 1)
	local result
	if type(t) == "table" then
		result = util.CloneArray(t)
		table.insert(result, value)
	else
		result = { value }
	end
	self.vars[key] = result
end

function Environment:Prepend(key, value)
	local t = self:GetList(key, 1)
	local result
	if type(t) == "table" then
		result = util.CloneArray(t)
		table.insert(result, 1, value)
	else
		result = { value }
	end
	self.vars[key] = result
end

function Environment:Set(key, value)
	assert(key:len() > 0, "Key must not be empty")
	assert(type(key) == "string", "Key must be a string")
	if type(value) == "string" then
		if value:len() > 0 then
			self.vars[key] = { value }
		else
			-- Let empty strings make empty tables
			self.vars[key] = {}
		end
	elseif type(value) == "table" then
		-- FIXME: should filter out empty values
		for _, v in ipairs(value) do
			if not type(v) == "string" then
				error("key " .. key .. "'s table value contains non-string value " .. tostring(v))
			end
		end
		self.vars[key] = util.CloneArray(value)
	else
		error("key " .. key .. "'s value is neither table nor string: " .. tostring(value))
	end
end

function Environment:GetId()
	return self.id
end

function Environment:Get(key, default)
	local v = self.vars[key]
	if v then
		return table.concat(v, " ")
	elseif self.parent then
		return self.parent:Get(key, default)
	elseif default then
		return default
	else
		error(string.format("Key '%s' not present in environment", key))
	end
end

function Environment:GetList(key, default)
	local v = self.vars[key]
	if v then
		return v -- FIXME: this should be immutable from the outside
	elseif self.parent then
		return self.parent:GetList(key, default)
	elseif default then
		return default
	else
		error(string.format("Key '%s' not present in environment", key))
	end
end

function Environment:GetParent()
	return self.parent
end

function Environment:Interpolate(str, vars)
	assert(type(str) == "string")
	assert(not vars or type(vars) == "table")

	local function gsub_all(t, pattern, replacement)
		local result = {}
		for idx, value in ipairs(t) do
			result[idx] = value:gsub(pattern, replacement)
		end
		return result
	end

	local function replace(str)
		local name
		local options

		-- Allow embedded colons escaped by backslashes by turning them into ascii code 1
		-- Ugly, but efficient. Motivated by e.g. $(LIBPATH:p/libpath\\:) to yield /libpath:a /libpath:b
		str = str:gsub("\\:", string.char(1))

		for match in str:gmatch(":([^:]+)") do
			options = options or {}
			match = match:gsub(string.char(1), ":")
			table.insert(options, match)
		end

		if options then
			local first_colon = str:find(':', 1, true) -- plain
			name = str:sub(1, first_colon-1)
		else
			name = str
		end

		-- First try the substitution table
		local v = vars and vars[name] or nil

		-- Then the environment dictionary
		if not v then
			v = self:GetList(name) -- this will never return nil
		else
			if type(v) ~= 'table' then
				v = { v }
			end
		end

		-- Adjust accoring to options
		local join_string = " "
		if options then
			for _, o in ipairs(options) do
				local first_char = o:sub(1, 1)
				if 'f' == first_char then
					v = gsub_all(v, '[/\\]', '/')
				elseif 'b' == first_char then
					v = gsub_all(v, '[/\\]', '\\')
				elseif 'p' == first_char then
					v = gsub_all(v, '^', o:sub(2))
				elseif 'a' == first_char then
					v = gsub_all(v, '$', o:sub(2))
				elseif 'j' == first_char then
					join_string = o:sub(2)
				else
					error("bad interpolation option " .. tostring(o) .. " in " .. str)
				end
			end
		end

		return table.concat(v, join_string)
	end

	local repeat_count = 1
	while repeat_count <= 10 do
		repeat_count = repeat_count + 1
		local replace_count
		str, replace_count = str:gsub("%$%(([^)]+)%)", replace)
		if 0 == replace_count then
			return str
		end
	end

	io.stderr:write("Couldn't interpolate \"", str, "\"\n")
	io.stderr:write("Environment:\n")
	for k, v in pairs(self.vars) do
		io.stderr:write(string.format("%20s = \"%s\"\n", k, v))
	end
	io.stderr:write("Vars:\n")
	for k, v in pairs(vars) do
		io.stderr:write(string.format("%20s = %s\n", util.tostring(k), util.tostring(v)))
	end
	error("Couldn't interpolate string " .. str) 
end

function Environment:MakeNode(values)
	return depgraph.CreateNode(self, values)
end

function Create(parent, assignments, obj)
	return Environment:Create(parent, assignments, obj)
end

function Environment:GenerateDotGraph(stream, memo)
	-- Memoize
	if not memo[self] then
		memo[self] = 1
	else
		return
	end

	local id = self:GetId()
	local first = true

	stream:write('rankdir=LR;\n')

	stream:write(id)
	stream:write(' [shape=record, color=blue3, rankdir=LR, label="')

	local fieldno = 1

	for k, v in pairs(self:GetVars()) do
		if not first then
			stream:write('|')
		end
		first = false
		stream:write("<f")
		stream:write(fieldno)
		stream:write("> ")
		stream:write(k)
		stream:write(' = ')
		stream:write(table.concat(v, " "))
		fieldno = fieldno + 1
	end

	stream:write('"];\n')

	local link = self:GetParent()
	assert(link ~= self)
	if link then
		link:GenerateDotGraph(stream, memo)
		stream:write(id)
		stream:write(" -> ")
		stream:write(link:GetId())
		stream:write(' [style=dotted, label="EnvLink"] ;\n')
	end
end

function Environment:Serialize(f, state)
	if state[self] then
		return self.id
	else
		state[self] = true
	end

	if self.parent then 
		self.parent:Serialize(f, state)
	end

	f:write("local ", self.id, " = ")
	f:write("tundra.environment.Create(")
	if self.parent then
		f:write(self.parent.id)
	else
		f:write("nil")
	end
	f:write(", {\n")
	for k, varray in pairs(self.vars) do
		f:write(string.format("\t[%q] = {", k))
		for _, v in ipairs(varray) do
			f:write(string.format("%q, ", v))
		end
		f:write("},\n")
	end
	f:write("})\n")
	
	return self.id
end
