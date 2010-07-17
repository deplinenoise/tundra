module(..., package.seeall)

function split(fn)
	local dir, file = fn:match("^(.*)[/\\]([^\\/]*)$")
	if not dir then
		return ".", fn
	else
		return dir, file
	end
end

function normalize(fn)
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

function join(dir, fn)
	return normalize(dir .. '/' .. fn)
end

function get_filename_dir(fn)
	return select(2, split(fn))
end

function get_extension(fn)
	return fn:match("(%.[^.]+)$") or ""
end

function drop_suffix(fn)
	return fn:match("^(.*)%.[^./\\]+$") or fn
end

function get_filename_base(fn)
	assert(fn, "nil filename")
	local _,_,stem = fn:find("([^/\\]+)%.[^.]*$")
	if stem then return stem end
	_,_,stem = fn:find("([^/\\]+)$")
	return stem
end

