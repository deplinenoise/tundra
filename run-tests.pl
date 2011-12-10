#! /usr/bin/env perl

use strict;
use warnings;
use lib qw(test);
use TundraTest qw(load_tests run_tests);

my $tundra_exe = 'tundra';

if (@ARGV) {
	$tundra_exe = shift;
}

load_tests "test";
run_tests $tundra_exe;
