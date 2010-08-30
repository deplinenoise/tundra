-- Copyright 2010 Andreas Fredriksson
--
-- This file is part of Tundra.
--
-- Tundra is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- Tundra is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with Tundra.  If not, see <http://www.gnu.org/licenses/>.

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
