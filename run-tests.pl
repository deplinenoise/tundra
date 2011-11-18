#! /usr/bin/env perl

use strict;
use warnings;
use lib qw(test);
use TundraTest qw(load_tests run_tests);

load_tests "test";
run_tests;
