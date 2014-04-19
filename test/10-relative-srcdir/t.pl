
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
      SourceDir = "!SRCDIR!",
			Sources = { "foo.c", "../bar.c", { SourceDir = "!SRCDIR!/nested", "moo.c" } },
		}
		Default "foo"
	end,
}
END

my $foo_c = <<END;
extern int bar(int);
extern int moo(int);
int main(int argc, char* argv[]) {
	return bar(0) + moo(0);
}
END

my $bar_c = <<END;
int bar(int i) {
	return i;
}
END

my $moo_c = <<END;
int moo(int i) {
	return i;
}
END

sub run_test {
  my $src_dir = shift;
  my $bf = $build_file;
  $bf =~ s/!SRCDIR!/$src_dir/gs;
	my $files = {
		'tundra.lua'       => $bf,
		'src/foo.c'        => $foo_c,
		'src/nested/moo.c' => $moo_c,
		'bar.c'            => $bar_c,
	};

	with_sandbox($files, sub {
		run_tundra 'foo-bar';
	});
}

deftest {
	name => "SourceDir test",
	procs => [
		"Relative path, no slash" => sub { run_test "src" },
		"Relative path, slash" => sub { run_test "src/" },
	],
};
