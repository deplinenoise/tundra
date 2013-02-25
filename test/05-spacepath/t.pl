use TundraTest qw($objectroot);

sub make_build_file($) {
	my $use_digest = shift;
	<<END;
require 'tundra.syntax.testsupport'
local native = require 'tundra.native'

Build {
	Configs = {
		Config {
			Name = "foo-bar",
			ReplaceEnv = {
				OBJECTROOT = "a b c"
			},
      SupportedHosts = { native.host_platform },
		}
	},
	Units = function()
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

sub run_test() {
	my $use_digest = shift;

	my $files = {
		"tundra.lua" => make_build_file($use_digest),
		"test.input" => $test_input1,
	};

	local $objectroot = 'a b c';

	with_sandbox($files, sub {
		run_tundra 'foo-bar';
		expect_output_contents 'test.output', uppercase($test_input1);
	});
}

deftest {
    name => "Paths with spaces",
    procs => [
		"Relative OBJECTROOT with spaces" => sub { run_test(); },
	]
};
