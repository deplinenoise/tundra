
local error_count = 0

function unit_test(label, fn)
	local t_mt = {
		check_equal = function (obj, a, b) 
			if a ~= b then
				io.stdout:write("\n    ", tostring(a) .. " != " .. tostring(b))
				error_count = error_count + 1
			end
		end
	}
	t_mt.__index = t_mt

	local t = setmetatable({}, t_mt)
	local function stack_dumper(err_obj)
		local debug = require('debug')
		return debug.traceback(err_obj, 2)
	end

	io.stdout:write("Testing ", label, ": ")
	io.stdout:flush()
	local ok, err = xpcall(function () fn(t) end, stack_dumper)
	if not ok then
		io.stdout:write("failed: ", tostring(err), "\n")
		error_count = error_count + 1
	else
		io.stderr:write("OK\n")
	end
end

dofile(TundraRootDir .. "/scripts/test/t_env.lua")

return error_count
