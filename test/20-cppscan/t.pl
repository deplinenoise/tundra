
my $build_file = <<END;
local native = require 'tundra.native'
Build {
	EngineOptions = {
		UseDigestSigning = 1,
	},
	Configs = {
		Config {
			Name = "foo-bar",
			Tools = { "gcc" },
      DefaultOnHost = { native.host_platform },
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

sub run_test($$) {
	my ($files, $alteration) = @_;

	with_sandbox($files, sub {
		run_tundra 'foo-bar';
		my $sig1 = md5_output_file 'foo';

		&$alteration();

		run_tundra 'foo-bar';
		my $sig2 = md5_output_file 'foo';
		fail "failed to rebuild when header changed" if $sig1 eq $sig2;
	});
}

sub test1() {
	run_test({
		'tundra.lua' => $build_file,
		'foo.c' => $foo_c,
		'include1.h' => "enum { X = 0 };\n"
	}, sub {
		update_file 'include1.h', "enum { X = 1 };\n";
	});
}

sub test2() {
	run_test({
		'tundra.lua' => $build_file,
		'foo.c' => $foo_c,
		'include1.h' => "\n\n#include \"include2.h\"\n",
		'include2.h' => "enum { X = 0 };\n"
	}, sub {
		update_file 'include2.h', "enum { X = 1 };\n";
	});
}

sub test3() {
	run_test({
		'tundra.lua' => $build_file,
		'foo.c' => $foo_c,
		'include1.h' => "\n\n#include \"foo/include2.h\"\n",
		'foo/include2.h' => "#include \"../include3.h\"\n",
		'include3.h' => "enum { X = 0 };\n"
	}, sub {
		update_file 'include3.h', "enum { X = 1 };\n";
	});
}

sub test4() {
	run_test({
		'tundra.lua' => $build_file,
		'foo.c' => $foo_c,
		'include1.h' => "\n\n#include \"foo/include2.h\"\n",
		'foo/include2.h' => "\n\n#include \"../bar/include3.h\"\n",
		'bar/include3.h' => "enum { X = 0 };\n"
	}, sub {
		update_file 'bar/include3.h', "enum { X = 1 };\n";
	});
}

sub test5() {
	run_test({
		'tundra.lua' => $build_file,
		'foo.c' => $foo_c,
		'include1.h' => "\n\n#include \"foo/a.h\"\n",
		'foo/a.h' => "\n\n#ifndef FOOA\n#define FOOA\n#include \"../foo/b.h\"\n#endif\n",
		'foo/b.h' => "\n\n#ifndef FOOB\n#define FOOB\n#include \"../foo/a.h\"\nenum { X = 0 };\n#endif\n",
	}, sub {
		update_file 'foo/b.h',
			"\n\n#ifndef FOOB\n#define FOOB\n#include \"../foo/a.h\"\nenum { X = 1 };\n#endif\n";
	});
}

deftest {
	name => "cpp include scanning",
	procs => [
		"First level" => \&test1,
		"Second level" => \&test2,
		"Parent directory" => \&test3,
		"Sibling directory" => \&test4,
		"Header cycle" => \&test5,
	],
};
