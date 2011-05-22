
sub make_build_file($) {
	my $use_digest = shift;
	<<END;
local util = require 'tundra.util'
local nodegen = require 'tundra.nodegen'

local function test_unit(generator, env, decl)
	return env:make_node {
		Label = "TestAction \$(@)",
		Action = "tr a-z A-Z < \$(<) > \$(@)",
		InputFiles = { decl.InputFile },
		OutputFiles = { decl.OutputFile },
	}
end

Build {
	UnitEval = {
		TestUnit = test_unit,
	},
	Configs = {
		Config {
			Name = "foo-bar",
		}
	},
	EngineOptions = {
		UseDigestSigning = $use_digest,
	},
	Units = function()
		TestUnit {
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
	my $use_digest = shift;

	my $files = {
		"tundra.lua" => make_build_file($use_digest),
		"test.input" => $test_input1,
	};

	with_sandbox($files, sub {
		run_tundra 'foo-bar';
		expect_output_contents 'test.output', uppercase($test_input1);

		update_file 'test.input', $test_input2;
		bump_timestamp 'test.input';
		run_tundra 'foo-bar';
		expect_output_contents 'test.output', uppercase($test_input2);
	});
}

sub test_timestamp { run_test(0); }
sub test_digest { run_test(1); }

deftest {
    name => "Basic",
    procs => [
		"Basic timestamp signing" => \&test_timestamp,
		"Basic digest signing" => \&test_digest,
	]
};
