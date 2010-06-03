
module(..., package.seeall)

local util = require("tundra.util")
local engine = require("tundra.native.engine")

local Node = { }
Node.__index = Node

local NodeByLabel = {}
local NodeIndex = {}
NodesFromDisk = {}

NodeType = {
	GraphGenerator = 1,
	ShellAction = 2,
}

TypeToString = {
	[1] = "GraphGenerator",
	[2] = "ShellAction",
}

function CreateNode(env_, data_)
	assert(env_)
	local node = {
		env = env_,
		node_type = data_.Type or NodeType.ShellAction,
		deps = {},
		depset = {},
		cachable = data_.Cachable,
		from_disk = data_.FromDisk,
	}

	assert(TypeToString[node.node_type])

	node.inputs = util.mapnil(data_.InputFiles, function (x) return engine.NormalizePath(env_:Interpolate(x)) end)
	node.outputs = util.mapnil(data_.OutputFiles, function (x) return engine.NormalizePath(env_:Interpolate(x)) end)

	local expand_env = {
		['<'] = node.inputs,
		['@'] = node.outputs
	}

	if data_.Action then
		node.action = data_.Action and env_:Interpolate(data_.Action, expand_env)
	else
		assert(not node.outputs, "can't have output files without an action")
	end

	if data_.Label then
		node.annotation = env_:Interpolate(data_.Label, expand_env)
	else
		node.annotation = "?"
	end

	--[[
	Compute a unique identifier for this node by hashing its action and the
	input and output files involved. This way is multiple build steps do the
	same thing, they will map to the same node in the graph and cause the subtree
	to be shared. One example of such sharing is when the same C++ source file
	is a part of multiple libraries. If no environment changes are made, it
	will have the same headers in all builds and the header scanning needs only
	be done once (by the same node).

	FIXME dep: If there are hidden dependencies here e.g. in the environment
	they will be lost. This could happen if environment variables such as
	INCLUDE affect certain toolsets. MSVC allows this even though it's not
	widely used (but it was on BF2).
	--]]
	do
		local hasher = engine.CreateHasher()
		-- Include the annotation to differentiate between top-level named
		-- nodes such as 'Clean' and 'All' that differ only by name.
		hasher:AddString(tostring(node.node_type))
		if node.annotation then hasher:AddString(node.annotation) end
		if node.action then hasher:AddString(node.action) end
		if node.inputs then for _, i in ipairs(node.inputs) do hasher:AddString(i) end end
		if node.outputs then for _, o in ipairs(node.outputs) do hasher:AddString(o) end end
		local id = hasher:GetDigest()

		local existing = NodeIndex[id]
		if existing then
			return existing
		end

		existing = NodesFromDisk[id]
		if existing then
			if Options.Verbose then
				io.stdout:write("Using disk cached copy of ", node.annotation, "\n")
			end
			NodeIndex[id] = existing
			return existing
		end

		node.id = id
		NodeIndex[node.id] = node
	end

	NodeByLabel[node.annotation] = node

	setmetatable(node, Node)

	if data_.Dependencies then
		for _, d in pairs(data_.Dependencies) do
			node:AddDependency(d)
		end
	end

	if data_.FromDisk then
		NodesFromDisk[node.id] = node
	end

	return node
end

function Node:GetAnnotation()
	return self.annotation
end

function Node:GetId()
	return self.id
end

function Node:GetAction()
	return self.action
end

function Node:AddDependency(dep)
	if not self.depset[dep] then
		table.insert(self.deps, dep)
		self.depset[dep] = 1
	end
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

function Node:GetSignature()
	return self.signature
end

function Node:HasVisisted()
	return self.visited
end

function Node:MarkAsVisisted()
	self.visited = 1
end

function Node:IsGenerator()
	return self.node_type == NodeType.GraphGenerator
end

function Node:GenerateDotGraph(stream, memo)
	-- Memoize this node
	if not memo[self] then
		memo[self] = 1
	else
		return
	end

	stream:write("node_", self:GetId(), " [")
	if self:IsGenerator() then
		stream:write("shape=box, style=rounded")
	else
		stream:write("shape=box")
	end
	if self.from_disk then
		stream:write(", color=red")
	end
	stream:write(", label=\"")
	stream:write(self:GetAnnotation())
	stream:write("\\n")

	do local action = self:GetAction()
		if action then
			if self:IsGenerator() then
				stream:write('(', action, ')')
			else
				stream:write("Command: ", action, "\\n")
			end
		end
	end

	stream:write("\"")

	stream:write("];\n")

	local function DumpFile(stream, fn, memo)
		local key = string.gsub(fn, '([^A-Za-z0-9])', function (x) return '_'..x:byte() end)
		if memo[fn] then
			return key
		end
		stream:write(key, " [color=brown, shape=note, label=\"", fn, "\"];\n")
		return key
	end

	local function DumpFiles(f, label)
		if f then
			for _, fn in ipairs(f) do
				local fkey = DumpFile(stream, fn, memo)
				stream:write('node_', self:GetId(), " -> ", fkey, " [label=\"", label, "\"];\n")
			end
		end
	end

	DumpFiles(self:GetInputFiles(), "Input")
	DumpFiles(self:GetOutputFiles(), "Output")

	-- Optionally emit environments
	if Options.GraphEnv then
		self.env:GenerateDotGraph(stream, memo)

		stream:write('node_', self:GetId(), ' -> ', self.env:GetId())
		stream:write(" [label=\"Env\", style=dotted];\n")
	end

	for _, dep in ipairs(self:GetDependencies()) do
		dep:GenerateDotGraph(stream, memo)
		stream:write('node_', self:GetId(), " -> ", 'node_', dep:GetId(), " [label=Depends];\n")
	end
end

function GenerateDotGraph(filename, nodes)
	local stream = assert(io.open(filename, 'w'), "can't open file for writing")
	stream:write('digraph all {\n')
	stream:write('node [fontname=tahoma, fontsize=10];\n')
	stream:write('edge [fontname=tahoma, fontsize=8];\n')
	local memo = {}
	for _, node in ipairs(nodes) do
		node:GenerateDotGraph(stream, memo)
	end
	stream:write('}\n')
	stream:close()
end

function Node:GetEnvironment()
	return self.env
end

function Node:GetInputSignature()
	local signature = {}

	signature.action = self:GetAction()

	signature.input_files = util.mapnil(self:GetInputFiles(), function (fn)
		local size, digest = engine.StatPath(fn)
		if size < 0 then
			-- Ignore missing input files when dry running
			if not Options.DryRun then
				error(self:GetAnnotation() .. ": missing input file " .. fn)
			end
		end
		return {
			['Filename']=fn,
			['Size']=size,
			['Digest']=digest,
			['Timestamp']=os.time(),
		}
	end)

	return signature
end

function SpliceOutputs(nodes)
	local result = {}
	for _, node in ipairs(nodes) do
		local o = node:GetOutputFiles()
		if o then
			for _, output in ipairs(o) do
				table.insert(result, output)
			end
		end
	end
	return result
end

function FindNode(name)
	assert(name)
	local n = NodeByLabel[name]
	if not n then
		error("node " .. name .. " not found")
	end
	return n
end

function PersistCachable(f, node, state)
	if state[node] then
		return
	else
		state[node] = true
		state.node_count = state.node_count + 1
	end

	assert(node.cachable)

	local env_id = node:GetEnvironment():Serialize(f, state)

	f:write(string.format("n = tundra.depgraph.CreateNode(%s, {\n", env_id))
	f:write("\tFromDisk = true,\n")
	f:write("\tCachable = true,\n")
	f:write(string.format("\tType = %d,\n", node.node_type))
	if node.annotation then
		f:write(string.format("\tLabel = %q,\n", node.annotation))
	end
	if node.action then
		f:write(string.format("\tAction = %q,\n", node.action))
	end
	if node.inputs then
		f:write("\tInputFiles = {\n")
		for _, i in ipairs(node.inputs) do
			f:write(string.format("\t\t%q,\n", i))
		end
		f:write("\t},\n")
	end
	if node.outputs then
		f:write("\tOutputFiles = {\n")
		for _, o in ipairs(node.outputs) do
			f:write(string.format("\t\t%q,\n", o))
		end
		f:write("\t},\n")
	end
	f:write("})\n")
end

function ReadNodeCache(fn)
	local function f()
		local chunk = assert(loadfile(fn))
		chunk()
	end
	local ok, errstr = pcall(f)
	if not ok then
		io.stderr:write(errstr, "\n")
	end
end

function WriteNodeCache(fn)
	local f = assert(io.open(fn, 'w'))
	local state = { node_count = 0 }
	f:write("local n\n")
	for id, node in pairs(NodeIndex) do
		if node.cachable then
			for _, d in util.NilIPairs(node.deps) do
				PersistCachable(f, d, state)
			end
			PersistCachable(f, node, state)
			for _, d in util.NilIPairs(node.deps) do
				f:write(string.format("n:AddDependency(assert(tundra.depgraph.NodesFromDisk[%q]))\n", d.id))
			end
		end
	end
	f:close()

	if Options.Verbose then
		io.stdout:write("Persisted ", state.node_count, " cachable nodes to ", fn)
	end
end

