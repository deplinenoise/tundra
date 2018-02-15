
my $build_file = <<END;
local native = require 'tundra.native'

Build {
  Configs = {
    Config {
      Name = "foo-bar",
      Tools = { "yasm", "gcc" },
      Env = {
        ASMINCPATH = {
          "incy",
        },
      },
      DefaultOnHost = { native.host_platform },
    }
  },
  Units = function()
    local result = ObjGroup {
      Name = "result",
      Sources = "foo.asm",
    }
    Default(result)
  end,
}
END


my $obj_file = '__result/foo-asm-51cc54dd91001f54ab1418e474e662f5.o';
#my $obj_file = '__result/foo-asm-0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33.o'; # SHA-1

my $foo_asm = <<END;
  %include "include1.i"
  global _main
_main:
  mov eax, RETURN_CODE
  ret
END

sub run_test($$) {
  my ($files, $alteration) = @_;

  with_sandbox($files, sub {
    run_tundra 'foo-bar';
    my $sig1 = md5_output_file $obj_file;

    &$alteration();

    run_tundra 'foo-bar';
    my $sig2 = md5_output_file $obj_file;
    fail "failed to rebuild when included file changed" if $sig1 eq $sig2;
  });
}

sub test1() {
  run_test({
    'tundra.lua' => $build_file,
    'foo.asm' => $foo_asm,
    'include1.i' => "RETURN_CODE EQU 0\n"
  }, sub {
    update_file 'include1.i', "RETURN_CODE EQU 1\n";
  });
}

sub test2() {
  run_test({
    'tundra.lua' => $build_file,
    'foo.asm' => $foo_asm,
    'include1.i' => "\n\n\t\t%include \"include2.i\"\n",
    'include2.i' => "RETURN_CODE EQU 0;\n"
  }, sub {
    update_file 'include2.i', "RETURN_CODE EQU 1;\n";
  });
}

sub test3() {
  run_test({
    'tundra.lua' => $build_file,
    'foo.asm' => $foo_asm,
    'include1.i' => "\n\n\t%include \"foo/include2.i\"\n",
    'foo/include2.i' => "\t%include \"../include3.i\"\n",
    'include3.i' => "RETURN_CODE EQU 0\n"
  }, sub {
    update_file 'include3.i', "RETURN_CODE EQU 1;\n";
  });
}

sub test4() {
  run_test({
    'tundra.lua' => $build_file,
    'foo.asm' => $foo_asm,
    'include1.i' => "\n\n\t%include \"foo/include2.i\"\n",
    'foo/include2.i' => "\n\n\t%include \"../bar/include3.i\"\n",
    'bar/include3.i' => "RETURN_CODE EQU 0\n"
  }, sub {
    update_file 'bar/include3.i', "RETURN_CODE EQU 1\n";
  });
}

sub test5() {
  run_test({
    'tundra.lua' => $build_file,
    'foo.asm' => "\t\tincbin \"bar.bin\"\n",
    'bar.bin' => "something\n"
  }, sub {
    update_file 'bar.bin', "something else\n";
  });
}

sub test6 {
  my $files = {
    'tundra.lua' => $build_file,
    'foo.asm' => "\t\tincbin \"bar.bin\"\n",
    'bar.bin' => "include 'bar.i'\n",
    'bar.i' => "whatever\n"
  };

  with_sandbox($files, sub {
    run_tundra 'foo-bar';
    my $sig1 = md5_output_file $obj_file;

    update_file 'bar.i', 'something else';

    run_tundra 'foo-bar';
    my $sig2 = md5_output_file $obj_file;
    fail "incbin file was scanned for dependencies" if $sig1 ne $sig2;
  });

}

sub test7 {
  run_test({
    'tundra.lua' => $build_file,
    'foo.asm' => "\t\tincbin \"header.i\"\n",
    'incy/header.i' => "\tmov eax, 1\n"
  }, sub {
    update_file 'incy/header.i', "\tmov eax, 2\n";
  });
}

deftest {
  name => "yasm (generic) include scanning",
  procs => [
    "First level" => \&test1,
    "Second level" => \&test2,
    "Parent directory" => \&test3,
    "Sibling directory" => \&test4,
    "Basic incbin" => \&test5,
    "incbin doesn't follow" => \&test6,
    "Incbin via ASMINCPATH" => \&test7,
  ],
};
