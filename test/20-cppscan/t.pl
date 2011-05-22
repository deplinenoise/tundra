
my $build_file = <<END;
Build {
	EngineOptions = {
		UseDigestSigning = 1,
	},
	Configs = {
		Config {
			Name = "foo-bar",
			Tools = { "gcc" },
		}
	},
	Units = function()
		Program {
			Name = "foo",
			Sources = "foo.c",
		}
		Default "foo"
	end,
}
END

my $foo_c = <<END;
#include <stdio.h>
#include "include1.h"

int main(int argc, char* argv[]) {
	return X;
}
END

sub test1() {
	my %files = (
		'tundra.lua' => $build_file,
		'foo.c' => $foo_c,
		'include1.h' => "enum { X = 0 };\n"
	);

	with_sandbox(\%files, sub {
		run_tundra 'foo-bar';
		my $sig1 = md5_output_file 'foo';
		update_file 'include1.h', "enum { X = 1 };\n";
		run_tundra 'foo-bar';
		my $sig2 = md5_output_file 'foo';
		fail "failed to rebuild when header changed" if $sig1 eq $sig2;
	});
}

sub test2() {
	my %files = (
		'tundra.lua' => $build_file,
		'foo.c' => $foo_c,
		'include1.h' => "\n\n#include \"include2.h\"\n",
		'include2.h' => "enum { X = 0 };\n"
	);

	with_sandbox(\%files, sub {
		run_tundra 'foo-bar';
		my $sig1 = md5_output_file 'foo';
		update_file 'include2.h', "enum { X = 1 };\n";
		run_tundra 'foo-bar';
		my $sig2 = md5_output_file 'foo';
		fail "failed to rebuild when 2nd level header changed" if $sig1 eq $sig2;

	});
}

deftest {
	name => "cpp include scanning",
	procs => [
		"First level" => \&test1,
		"Second level" => \&test2,
	],
};
