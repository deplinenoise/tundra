
sub make_build_file($) {
	my $use_caching = shift;
	<<END;
local native = require 'tundra.native'
require 'tundra.syntax.testsupport'
Build {
	Configs = {
		Config {
			Name = "foo-bar",
      DefaultOnHost = { native.host_platform },
		}
	},
	Passes = {
		MyPass = { Name = "Test", BuildOrder = 1 },
	},
	EngineOptions = {
		UseDagCaching = $use_caching,
	},
	Units = function()
		UpperCaseFile {
			Pass = "MyPass",
			Name = "bar",
			InputFile = "test2.input",
			OutputFile = "\$(OBJECTDIR)/test2.output",
		}
		UpperCaseFile {
			Name = "foo",
			InputFile = "test.input",
			OutputFile = "\$(OBJECTDIR)/test.output",
		}
		Default "foo"
	end,
}
END
}

my $test_input1 = "this is the test input";
my $test_input2 = "this is the test input after modification";

sub uppercase {
    my $s = shift;
    $s =~ tr/a-z/A-Z/;
    return $s;
}

sub run_test($) {
	my $use_caching = shift;
	my $files = {
		"tundra.lua" => make_build_file($use_caching),
		"test.input" => $test_input1,
		"test2.input" => $test_input1,
	};

	with_sandbox($files, sub {
		run_tundra('foo-bar', 'foo');
		expect_output_contents 'test.output', uppercase($test_input1);
		fail "bar target was built" if output_file_exists 'test2.output';
	});
}

deftest {
    name => "Passes",
    procs => [
		"Unselected early-pass nodes do not build" => sub { run_test(0); },
		"Unselected early-pass nodes do not build (DAG cache)" => sub { run_test(1); },
	]
};
