module(..., package.seeall)

local util = require("tundra.util")
local path = require("tundra.path")

local Node = {}
local NodeMeta = { __index = Node }

function CreateNode(env_, data_)
	assert(env_)
	local node = setmetatable({
		env = env_,
		deps = {},
	}, NodeMeta)

	node.inputs = util.mapnil(data_.InputFiles, function (x) return path.NormalizePath(env_:Interpolate(x)) end)
	node.outputs = util.mapnil(data_.OutputFiles, function (x) return path.NormalizePath(env_:Interpolate(x)) end)

	local expand_env = {
		['<'] = node.inputs,
		['@'] = node.outputs
	}

	if data_.Action then
		node.action = env_:Interpolate(data_.Action, expand_env)
	else
		assert(not node.outputs, "can't have output files without an action")
	end

	if data_.Label then
		node.annotation = env_:Interpolate(data_.Label, expand_env)
	else
		node.annotation = "?"
	end

	if data_.Dependencies then
		for _, d in pairs(data_.Dependencies) do
			node:AddDependency(d)
		end
	end

	return node
end

function Node:GetAnnotation()
	return self.annotation
end

function Node:GetAction()
	return self.action
end

function Node:AddDependency(dep)
	for _, old_dep in ipairs(self.deps) do
		if old_dep == dep then
			return
		end
	end

	table.insert(self.deps, dep)
end

function Node:AddDependencies(deps)
	for _, dep in ipairs(self.deps) do
		self:AddDependency(dep)
	end
end

function Node:GetDependencies()
	return self.deps
end

function Node:GetInputFiles()
	return self.inputs
end

function Node:GetOutputFiles()
	return self.outputs
end

function Node:GetEnvironment()
	return self.env
end

function SpliceOutputs(nodes, exts)
	local result = {}
	for _, node in ipairs(nodes) do
		local o = node:GetOutputFiles()
		if o then
			for _, output in ipairs(o) do
				table.insert(result, output)
			end
		end
	end

	if exts then
		util.FilterInPlace(result, function (fn)
			local ext = path.GetExtension(fn)
			for idx = 1, #exts do
				if ext == exts[idx] then
					return true
				end
			end
			return false
		end)
	end
	print("RES:", util.tostring(result))
	return result
end

