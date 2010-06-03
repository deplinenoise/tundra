

unit_test('scalar interpolation', function (t)
	local e = require('tundra.environment')
	local e1, e2, e3
	e1 = e.Create(nil, { Foo="Foo", Baz="Strut" })
	e2 = e1:Clone({ Foo="Bar" })
	e3 = e1:Clone({ Baz="c++" })

	t:CheckEqual(e1:Get("Foo"), "Foo")
	t:CheckEqual(e1:Get("Baz"), "Strut")
	t:CheckEqual(e2:Get("Foo"), "Bar")
	t:CheckEqual(e2:Get("Baz"), "Strut")
	t:CheckEqual(e3:Get("Fransos", "Ost"), "Ost")

	t:CheckEqual(e1:Interpolate("$(Foo) $(Baz)"), "Foo Strut")
	t:CheckEqual(e2:Interpolate("$(Foo) $(Baz)"), "Bar Strut")
	t:CheckEqual(e3:Interpolate("$(Foo) $(Baz)"), "Foo c++")
	t:CheckEqual(e1:Interpolate("a $(>)", { ['>'] = "foo" }), "a foo")
end)

unit_test('list interpolation', function (t)
	local e = require('tundra.environment')
	local e1 = e.Create()

	e1:Set("Foo", "Foo")
	t:CheckEqual(e1:Interpolate("$(Foo)"), "Foo")

	e1:Set("Foo", { "Foo" })
	t:CheckEqual(e1:Interpolate("$(Foo)"), "Foo")

	e1:Set("Foo", { "Foo", "Bar" } )
	t:CheckEqual(e1:Interpolate("$(Foo)") , "Foo Bar")
	t:CheckEqual(e1:Interpolate("$(Foo:j,)"), "Foo,Bar")
	t:CheckEqual(e1:Interpolate("$(Foo:p!)") , "!Foo !Bar")
	t:CheckEqual(e1:Interpolate("$(Foo:a!)") , "Foo! Bar!")
	t:CheckEqual(e1:Interpolate("$(Foo:p-I:j__)") , "-IFoo__-IBar")
	t:CheckEqual(e1:Interpolate("$(Foo:j\\:)"), "Foo:Bar")
end)
