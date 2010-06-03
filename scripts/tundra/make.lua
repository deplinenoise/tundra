module(..., package.seeall)

local depgraph = require("tundra.depgraph")
local util = require("tundra.util")
local engine = require("tundra.native.engine")

local OldSignatures = {}
local NewSignatures = {}

local function CheckSignature(old_sig, new_sig)
	if not old_sig then
		return false, "No signature"
	end

	if old_sig.result ~= 0 then
		return false, "Last build failed"
	end

	-- Check input files
	do
		local old_inputs = old_sig.input_files
		local new_inputs = new_sig.input_files

		if (old_inputs and #old_inputs or 0) ~= (new_inputs and #new_inputs or 0) then
			return false, "Number of inputs changed"
		end

		if old_inputs then
			for i=1, #old_inputs do
				local o = old_inputs[i]
				local n = new_inputs[i]
				if o.Digest ~= n.Digest then
					return false, "Input " .. o.Filename .. " changed"
				end
			end
		end
	end

	-- Check output files
	if old_sig.output_files then
		for _, out in ipairs(old_sig.output_files) do
			local size, digest = engine.StatPath(out.Filename)
			if size < 0 then
				return false, "Output file " .. out.Filename .. " missing"
			end
		end
	end

	return true
end

function ReadSignatureDb()
	-- FIXME: should have a better concept of where to read it from
	local success, result = pcall(function() return dofile("tundra.db") end)
	if success then
		if result then
			OldSignatures = result
			NewSignatures = util.CloneTable(OldSignatures)
		else
			io.stderr:write("warning: couldn't load signatures: nil result\n")
		end
	else
		io.stderr:write("warning: couldn't load signatures: ", result, "\n")
	end
end

function WriteSignatures()
	-- FIXME: should have a better concept of where to write it
	f = io.open("tundra.db", "w")
	util.Serialize(f, NewSignatures)
	f:close()
end

function Make(node, depth, state, dirty)
	state = state or {}
	dirty = dirty or {}
	assert(node)

	depth = depth or 0

	local verbose = Options.Verbose
	local indent

	if verbose then
		indent = string.rep("  ", depth)
	end

	-- If this node is a graph generator, have it create the graph now before
	-- we visit it.

	if node:IsGenerator() then
		if node.from_disk then
			if verbose then
				io.stdout:write(indent, "GraphGen <", node:GetAnnotation(), "> -- cached\n")
			end
		else
			if verbose then
				io.stdout:write(indent, "GraphGen <", node:GetAnnotation(), ">\n")
			end
			local action = node:GetAction()
			if action:sub(1, 4) == "lua " then
				local fn_name = action:sub(5)
				local fn = assert(_G[fn_name])
				fn(node)
			else
				error("unsupported generator action: " .. action)
			end
		end
	else
		if verbose then
			io.stdout:write(indent, "Visit <", node:GetAnnotation(), ">\n")
		end
	end

	local dependencies_rebuilt = false

	-- Ensure dependencies are up to date
	for _, dep in ipairs(node:GetDependencies()) do
		if not state[dep] then
			state[dep] = 1
			local result, did_work = Make(dep, depth+1, state, dirty)
			if not result then
				return result, false
			end
			if did_work then
				dirty[dep] = true
				dependencies_rebuilt = true
			end
			state[dep] = nil
		else
			dependencies_rebuilt = dependencies_rebuilt or dirty[dep]
		end
	end

	-- See if we need to do anything
	local id = node:GetId()
	local new_signature = node:GetInputSignature()
	local old_signature = OldSignatures[id]

	local up_to_date, rebuild_reason

	if dependencies_rebuilt then
		up_to_date, rebuild_reason = false, 'Dependencies updated'
	else
		up_to_date, rebuild_reason = CheckSignature(old_signature, new_signature)
	end

	if up_to_date then
		if verbose then
			io.stdout:write(indent, node:GetAnnotation(), ": Up to date\n")
			NewSignatures[id] = old_signature
		end
		return true, false
	elseif verbose then
		io.stdout:write(indent, node:GetAnnotation(), ": ", rebuild_reason, "\n")
	end

	-- Make the target
	if not Options.DryRun then
		if new_signature.action and not node:IsGenerator() then
			io.stdout:write(node:GetAnnotation(), "\n")
			if Options.Verbose then
				io.stdout:write(new_signature.action, "\n")
			end
			new_signature.result = os.execute(new_signature.action)
		else
			if verbose then
				io.stdout:write(node:GetAnnotation(), " (no action)\n")
			end
			new_signature.result = 0
		end

		if 0 ~= new_signature.result then 
			io.stderr:write(node:GetAnnotation(), " failed with error code ", tostring(new_signature.result), "\n")
		else
			-- Pick up output files
			local output_files = node:GetOutputFiles()
			if output_files then
				local f = {}
				for _, ofn in ipairs(output_files) do
					local size, digest = engine.StatPath(ofn)
					if size < 0 then
						error(node:GetAnnotation() .. ": output file " .. ofn .. " was not built")
					else
						table.insert(f, { Filename=ofn, Digest=digest, Size=size })
					end
				end
				new_signature.output_files = f
			end
		end

		-- Persist the signature
		NewSignatures[id] = new_signature
	else
		local action = node:GetAction()
		if action then
			io.stdout:write(action, "\n")
		end
		return true, true
	end

	return new_signature.result == 0, true
end

