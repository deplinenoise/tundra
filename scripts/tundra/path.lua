module(..., package.seeall)

local engine = require("tundra.native.engine")

function SplitPath(fn)
	local dir, file = fn:match("^(.*)[/\\]([^\\/]*)$")
	if not dir then
		return ".", fn
	else
		return dir, file
	end
end

NormalizePath = engine.NormalizePath

function JoinPath(dir, fn)
	return engine.NormalizePath(dir, fn)
end

function GetFilenameDir(fn)
	return select(2, SplitPath(fn))
end

function GetExtension(fn)
	local _,_,ext = fn:find("%.([^.]+)$")
	return ext
end

function GetFilenameBase(fn)
	assert(fn, "nil filename")
	local _,_,stem = fn:find("([^/\\]+)%.[^.]*$")
	if stem then return stem end
	_,_,stem = fn:find("([^/\\]+)$")
	return stem
end

