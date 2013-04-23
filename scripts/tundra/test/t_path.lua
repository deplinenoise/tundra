module(..., package.seeall)

local path = require "tundra.path"

local function check_path(t, p, expected)
  p = p:gsub('\\', '/')
  t:check_equal(p, expected)
end

unit_test('path.normalize', function (t)
  check_path(t, path.normalize("foo"), "foo")
  check_path(t, path.normalize("foo/bar"), "foo/bar")
  check_path(t, path.normalize("foo//bar"), "foo/bar")
  check_path(t, path.normalize("foo/./bar"), "foo/bar")
  check_path(t, path.normalize("foo/../bar"), "bar")
  check_path(t, path.normalize("../bar"), "../bar")
  check_path(t, path.normalize("foo/../../bar"), "../bar")
end)

unit_test('path.join', function (t)
  check_path(t, path.join("foo", "bar"), "foo/bar")
  check_path(t, path.join("foo", "../bar"), "bar")
end)
