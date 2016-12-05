package TundraTest;

use strict;
use warnings;

use File::Find;
use File::Slurp;
use File::Temp qw(tempdir tempfile);
use File::Path qw(make_path remove_tree);
use File::Spec::Functions qw(splitpath catdir catpath);
use File::stat;
use Digest::MD5 qw(md5_hex);

BEGIN {
    use Exporter ();
    our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, $tundra_path, @tests, $testdir, $objectroot);

    $VERSION = 1.00;
    @ISA = qw(Exporter);
    @EXPORT = qw(
    &deftest &run_tundra &expect_contents &expect_output_contents
    &output_file_exists
    &update_file &with_sandbox &bump_timestamp
    &md5_output_file
    &fail);
    @EXPORT_OK = qw(&load_tests &run_tests $objectroot);
}

our $DEBUG = 0;
our $tundra_path = 'tundra2';
our $tundra_options = '-D -v -w';
our @tests = ();
our $testdir = "";
our $curr_config = "";
our $curr_output_dir = "";
our $curr_script = "";
our $keep_sandbox = 0;
our $objectroot = 't2-output';
our $last_run_time = 0;

sub fail($) {
  my $msg = shift;
  die $msg . "\n";
}

sub sandbox_path($) {
  my $fn = shift;
  catdir($testdir, $fn);
}

sub sandbox_dir_of($) {
  my $fn = shift;
  my ($vol, $dir, $file) = splitpath($fn);
  catdir($testdir, $dir);
}

sub output_path($) {
  my $fn = shift;
  catdir($curr_output_dir, $fn)
}

sub with_sandbox($$) {
    my ($filehash, $proc) = @_;
  local $testdir = tempdir();
  my $result = eval {
    foreach my $fn (keys %$filehash) {
      update_file($fn, $filehash->{$fn});
    }
    &$proc();
  };
  if ($keep_sandbox) {
    print "Sandbox left at $testdir\n";
  } else {
    remove_tree($testdir)
  }
  die $@ if $@;
}

sub run_tundra($;$) {
  my $config = shift;
  my $args = shift;
  $args = "" unless defined $args;

  $last_run_time = time();

  my $cmdline = "$tundra_path $tundra_options -C $testdir $config";
  my $pid = open (my $child, "-|", "$cmdline 2>&1");
  fail "cannot start tundra at $tundra_path: $!" unless defined($pid);

  my @output;
  while (<$child>) {
    push @output, $_;
  }
  close($child);

  my $rc = $?;
  unless ($rc == 0) {
    my $separator = ("=" x 79) . "\n";
    push @output, $separator;
    unshift @output, $separator;
    unshift @output, "\ntundra failed with result code $rc\n";
    fail (join("", @output) . "\n");
  }

  # Store away config & output dir for convenience later when checking results.
  $curr_config = $config;
  $curr_output_dir = catdir($objectroot, $curr_config . '-debug-default');
}

sub expect_contents($$) {
    my ($fn, $expected_data) = @_;
  my $path = sandbox_path($fn);

  fail "'$fn' was not generated" unless -e $path;
  fail "'$fn' is not a file" unless -f $path;

  my $data = read_file($path);

  fail "'$fn': unexpected content" unless $data eq $expected_data;
}

sub expect_output_contents($$) {
    my ($fn, $expected_data) = @_;
  return expect_contents(output_path($fn), $expected_data);
}

sub output_file_exists($) {
  my $fn = shift;
  return -f sandbox_path(output_path($fn));
}

sub md5_output_file($) {
    my $fn = shift;
  my $path = sandbox_path(output_path($fn));

  fail "'$fn' ($path) was not generated" unless -e $path;
  fail "'$fn' ($path) is not a file" unless -f $path;

  return md5_hex(read_file($path));
}

sub update_file($$) {
    my ($fn, $data) = @_;

  # Delay execution so that timestamps will differ
  sleep 1 while time() <= $last_run_time + 1;

  print "UPDATE: $fn\n" if $DEBUG;

  make_path(sandbox_dir_of($fn));

  my $targfn = sandbox_path($fn);
  open O, '>', $targfn;
  binmode O;
  print O $data;
  close O;
}

sub bump_timestamp($) {
  my $fn = shift;
  my $targfn = sandbox_path($fn);
  my $sb = stat $targfn;
  my $atime = $sb->atime;
  my $mtime = $sb->mtime;
  utime($atime + 1, $mtime + 1, $targfn)
    or warn "couldn't touch $targfn: $!";
}

sub wrap_script($$) {
  my ($pkgname, $script) = @_;
 <<END;
package $pkgname;

use strict;
use warnings;
use TundraTest;

$script

1;
END
}

sub deftest($) {
  my $t = shift;
  $t->{path} = $curr_script;
  push @tests, $t;
}

sub load_tests($) {
  my $dir = shift;
  my $test_idx = 0;

  my $visit_test = sub {
    if ($_ eq "t.pl" && -f) {
      my $pkgname = sprintf "tundra_test%08x", $test_idx++;
      my $script = wrap_script $pkgname, read_file($_);
      local $curr_script = "${File::Find::dir}/$_";
      my $result = eval $script;
      unless ($result) {
        print STDERR "couldn't load test ${File::Find::dir}/$_: $@\n";
        exit 1;
      }
    }
  };

  File::Find::find($visit_test, "test");
}

sub run_tests($\@) {
  local $tundra_path = shift;
  my $filter = shift;
  my %filter_hash = ();
  my $have_filter = 0;
  my $group_count = scalar(@tests);
  my ($test_count, $pass_count) = (0, 0);
  printf "Running %d test group%s\n", $group_count, $group_count == 1 ? "" : "s";

  foreach my $f (@$filter) {
    $have_filter = 1;
    $filter_hash{$f} = 1;
  }

  foreach my $t (@tests) {
    if ($have_filter and not $filter_hash{$t->{path}}) {
      print "\nSkipping $t->{name} ($t->{path})\n";
      next;
    }
    my ($gtest_count, $gpass_count) = (0, 0);
    print "\nGroup: $t->{name}\n";
    my $procs = $t->{procs};
    my $elem_count = scalar(@$procs);
    for (my $i = 0; $i < $elem_count; $i += 2) {
      ++$gtest_count;
      my ($name, $function) = ($procs->[$i], $procs->[$i+1]);
      print "- $name... ";
      eval { &$function(); };
      if ($@) {
        print "FAILED\n";
        print $@;
      } else {
        ++$gpass_count;
        print "OK\n";
      }
    }

    $test_count += $gtest_count;
    $pass_count += $gpass_count;

    if ($gtest_count > 0) {
      printf "Group summary: %d of %d passed (%.2f%%)\n",
      $gpass_count, $gtest_count, 100.0 * $gpass_count / $gtest_count;
    }
  }

  if ($test_count > 0) {
    printf "\nSummary: %d of %d tests passed (%.2f%%)\n",
    $pass_count, $test_count, 100.0 * $pass_count / $test_count;
  } else {
    print "\nNo tests were run. Maybe your filter was too good?\n";
  }
}

1;
