module(..., package.seeall)

function SplitPath(fn)
	local dir, file = fn:match("^(.*)[/\\]([^\\/]*)$")
	if not dir then
		return ".", fn
	else
		return dir, file
	end
end

function NormalizePath(fn)
	local pieces = {}
	for piece in string.gmatch(fn, "[^/\\]+") do
		if piece == ".." then
			table.remove(pieces)
		elseif piece == "." then
			-- nop
		else
			pieces[#pieces + 1] = piece
		end
	end
	return table.concat(pieces, '/')
end

function JoinPath(dir, fn)
	return engine.NormalizePath(dir, fn)
end

function GetFilenameDir(fn)
	return select(2, SplitPath(fn))
end

function GetExtension(fn)
	return fn:match("(%.[^.]+)$") or ""
end

function DropSuffix(fn)
	return fn:match("^([^.]*)%.[^.]+") or fn
end

function GetFilenameBase(fn)
	assert(fn, "nil filename")
	local _,_,stem = fn:find("([^/\\]+)%.[^.]*$")
	if stem then return stem end
	_,_,stem = fn:find("([^/\\]+)$")
	return stem
end

