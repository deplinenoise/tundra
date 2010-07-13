module(..., package.seeall)

local decl_meta = {}
decl_meta.__index = decl_meta

function make_decl_env()
	local obj = {
		Platforms = {},
		Projects = {},
		ProjectTypes = {},
		SourceGen = {},
		Results = {},
	}

	local outer_env = _G
	local function indexfunc(tab, var)
		-- Project types evaluate to functions that just store results
		local p
		p = obj.ProjectTypes[var]
		if p then
			if type(p) == "function" then
				return p
			else
				return function (data)
					obj.Results[#obj.Results + 1] = { Type = var, Decl = data }
				end
			end
		end

		-- Platform names evaluate to themselves
		if obj.Platforms[var] then return var end

		local fn = obj.SourceGen[var]
		if fn then return fn end

		return outer_env[var]
	end

	obj.FunctionMeta = { __index = indexfunc }
	obj.FunctionEnv = setmetatable({}, obj.FunctionMeta)

	return setmetatable(obj, decl_meta)
end

function decl_meta:add_platform(platform)
	self.Platforms[platform] = true
end

function decl_meta:add_project_type(name, fn)
	if fn then
		self.ProjectTypes[name] = setfenv(fn, self.FunctionEnv)
	else
		self.ProjectTypes[name] = true
	end
end

function decl_meta:add_source_generator(name, fn)
	self.SourceGen[name] = fn
end

function decl_meta:parse(file)
	local chunk = assert(loadfile(file))

	setfenv(chunk, self.FunctionEnv)
	chunk()
	return self.Results
end
