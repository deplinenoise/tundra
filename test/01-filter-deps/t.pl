sub make_build_file($) {
	my $use_digest = shift;
	<<END;
require 'tundra.syntax.testsupport'
local native = require 'tundra.native'

Build {
	Configs = {
		Config {
			Name = "foo-bar",
      SupportedHosts = { native.host_platform },
		}
	},
	EngineOptions = {
		UseDigestSigning = $use_digest,
	},
	Units = function()
		local s0 = UpperCaseFile {
			Name = "s0",
			InputFile = "test.input",
			OutputFile = "\$(OBJECTDIR)/test1.output",
		}
		local s1 = UpperCaseFile {
		    Depends = { { s0 } },
			Name = "s1",
			InputFile = "test.input",
			OutputFile = "\$(OBJECTDIR)/test2.output",
		}
		Default(s1)
	end,
}
END
}

my $test_input1 = "this is the test input";
my $test_input2 = "this is the test input after modification";

sub uppercase {
	uc $_[0];
}

sub run_test($) {
	my $use_digest = shift;

	my $files = {
		"tundra.lua" => make_build_file($use_digest),
		"test.input" => $test_input1,
	};

	with_sandbox($files, sub {
		run_tundra 'foo-bar';
		expect_output_contents 'test1.output', uppercase($test_input1);
		expect_output_contents 'test2.output', uppercase($test_input1);
	});
}

deftest {
    name => "Depends Filtering",
    procs => [
		"Test" => sub { run_test(0); },
	]
};

