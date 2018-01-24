#! /usr/bin/env perl

use strict;
use warnings;
use lib qw(test);
use TundraTest qw(load_tests run_tests);

my $tundra_exe = 'tundra';
my @tests = ();

if (@ARGV) {
	$tundra_exe = shift;
}

@tests = @ARGV;

load_tests "test";
my $rc = run_tests $tundra_exe, @ARGV;
exit $rc;
