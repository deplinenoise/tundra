module(..., package.seeall)

local util = require('tundra.util')
local path = require('tundra.path')
local depgraph = require('tundra.depgraph')

--[==[

The environment is a holder for variables and their associated values. Values
are always kept as tables, even if there is only a single value.

FOO = { a b c }

e:interpolate("$(FOO)") -> "a b c"
e:interpolate("$(FOO:j=, )") -> "a, b, c"
e:interpolate("$(FOO:p=-I)") -> "-Ia -Ib -Ic"

Missing keys trigger errors unless a default value is specified.

]==]--

local envclass = {}

function envclass:create(parent, assignments, obj)
	obj = obj or {}
	setmetatable(obj, self)
	self.__index = self

	obj.vars = {}
	obj.parent = parent
	obj.memos = {}
	obj.memo_keys = {}

	-- set up the table of make functions
	obj._make = {} 
	obj.make = setmetatable({}, {
		__index = function(table, key) return obj:_lookup_make(key) end,
		__newindex = function(table, key, value) obj:register_make_fn(key, value) end
	})

	-- assign initial bindings
	if assignments then
		obj:set_many(assignments)
	end

	return obj
end

function envclass:clone(assignments)
	return envclass:create(self, assignments)
end

function envclass:_lookup_make(name, real_env)
	real_env = real_env or self
	local entry = self._make[name]
	if entry then
		assert(entry.Name == name)
		local fn = function(args)
			return entry.Function(real_env, args)
		end
		return fn
	elseif self.parent then
		return self.parent:_lookup_make(name, real_env)
	else
		error("'" .. name .. "': no such make function", 2)
	end
end

function envclass:register_make_fn(name, fn, docstring)
	assert(type(name) == "string")
	assert(type(fn) == "function")
	self._make[name] = {
		Name = name,
		Function = fn,
		Doc = docstring or ""
	}
end

function envclass:register_implicit_make_fn(ext, fn, docstring)
	assert(type(ext) == "string")
	assert(type(fn) == "function")
	if not ext:match("^%.") then
		ext = "." .. ext -- we want the dot in the extension
	end

	if not self._implicit_exts then
		self._implicit_exts = {}
	end
	self._implicit_exts[ext] = {
		Function = fn,
		Doc = docstring or "",
	}
end

function envclass:get_implicit_make_fn(filename)
	local ext = path.get_extension(filename)
	local chain = self
	while chain do
		local t = chain._implicit_exts
		if t then
			local v = t[ext]
			if v then return v.Function end
		end
		chain = chain.parent
	end
	error(string.format("%s: no implicit make function for ext %s", filename, ext), 2)
end

function envclass:has_key(key)
	return self.vars[key] and true or false
end

function envclass:get_vars()
	return self.vars
end

function envclass:set_many(table)
	for k, v in pairs(table) do
		self:set(k, v)
	end
end

function envclass:append(key, value)
	self:invalidate_memos(key)
	local t = self:get_list(key, 1)
	local result
	if type(t) == "table" then
		result = util.clone_array(t)
		table.insert(result, value)
	else
		result = { value }
	end
	self.vars[key] = result
end

function envclass:prepend(key, value)
	self:invalidate_memos(key)
	local t = self:get_list(key, 1)
	local result
	if type(t) == "table" then
		result = util.clone_array(t)
		table.insert(result, 1, value)
	else
		result = { value }
	end
	self.vars[key] = result
end

function envclass:invalidate_memos(key)
	local name_tab = self.memo_keys[key]
	if name_tab then
		for name, _ in pairs(name_tab) do
			self.memos[name] = nil
		end
	end
end

function envclass:set(key, value)
	self:invalidate_memos(key)
	assert(key:len() > 0, "key must not be empty")
	assert(type(key) == "string", "key must be a string")
	if type(value) == "string" then
		if value:len() > 0 then
			self.vars[key] = { value }
		else
			-- let empty strings make empty tables
			self.vars[key] = {}
		end
	elseif type(value) == "table" then
		-- FIXME: should filter out empty values
		for _, v in ipairs(value) do
			if not type(v) == "string" then
				error("key " .. key .. "'s table value contains non-string value " .. tostring(v))
			end
		end
		self.vars[key] = util.clone_array(value)
	else
		error("key " .. key .. "'s value is neither table nor string: " .. tostring(value))
	end
end

function envclass:get_id()
	return self.id
end

function envclass:get(key, default)
	local v = self.vars[key]
	if v then
		return table.concat(v, " ")
	elseif self.parent then
		return self.parent:get(key, default)
	elseif default then
		return default
	else
		error(string.format("key '%s' not present in environment", key))
	end
end

function envclass:get_list(key, default)
	local v = self.vars[key]
	if v then
		return v -- FIXME: this should be immutable from the outside
	elseif self.parent then
		return self.parent:get_list(key, default)
	elseif default then
		return default
	else
		error(string.format("key '%s' not present in environment", key))
	end
end

function envclass:get_parent()
	return self.parent
end

function envclass:interpolate(str, vars)
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
			v = self:get_list(name) -- this will never return nil
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

	io.stderr:write("couldn't interpolate \"", str, "\"\n")
	io.stderr:write("env:\n")
	for k, v in pairs(self.vars) do
		io.stderr:write(string.format("%20s = \"%s\"\n", k, v))
	end
	io.stderr:write("vars:\n")
	for k, v in pairs(vars) do
		io.stderr:write(string.format("%20s = %s\n", util.tostring(k), util.tostring(v)))
	end
	error("couldn't interpolate string " .. str) 
end

function envclass:make_node(values)
	return depgraph.create_node(self, values)
end

function create(parent, assignments, obj)
	return envclass:create(parent, assignments, obj)
end

function envclass:record_memo_var(key, name)
	local tab = self.memo_keys[key]
	if not tab then
		tab = {}
		self.memo_keys[key] = tab
	end
	tab[name] = true
end

function envclass:memoize(key, name, fn)
	local memo = self.memos[name]
	if not memo then 
		self:record_memo_var(key, name)
		memo = fn()
		self.memos[name] = memo
	end
	return memo
end
