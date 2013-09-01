#! /usr/bin/env perl

# Script to parse a tundra.prof file and generate a human readable profile
# summary.
#
# This is awful, but I'd rather have it here in a script than in the C++ code.

use strict;
use warnings;
use Carp;
use Carp::Always;
use File::Slurp qw(read_file);

$SIG{__DIE__} = \&confess;
$SIG{__WARN__} = \&confess;

while (<>) {
  last if /^Functions:/;
}

my %functions;

while (<>) {
  last if /^Invocations:/;
  die "bad input: $_" unless /^\s*((?:0x)?[0-9A-Fa-f]+)\s+([^;]*);([^;]*);([^;]*);(-?\d+)\s*$/;
  die "duplicate function: $_" if $functions{$1};
  my $data = {
    id => $1,
    name => $2,
    call_count => 0,
    exclusive_time => 0,
  };

  my ($loc, $line) = ($4, $5);

  if ($data->{name} eq '') {
    $data->{name} = 'unknown';
  }

  my $locstr;

  if ($loc eq "=[C]") {
    $locstr = "C";
  } elsif ($loc =~ m!^@.*?([-a-zA-Z_0-9]+\.lua)$!) {
    $locstr = "$1:$line";
  } else {
    $locstr = $loc;
  }

  $data->{loc} = $locstr;

  $functions{$data->{id}} = $data;
}

my $call_tree = { function => undef, total_time => 0.0, calls => 0, own_time => 0.0, children => {} };
my $chain_count = 0;

while (<>) {
  die "bad input: $_" unless /^([^:]+): calls=(\d+) time=(\S+)\s*$/;
  ++$chain_count;
  my ($stack, $calls, $time) = ($1, $2, $3);
  $time *= 1000.0; # Convert to millis
  my @func_ids = reverse (split /<=/, $stack);
  my $node = $call_tree;
  my $leaf_func = undef;
  foreach (@func_ids) {
    my $func = $functions{$_};
    $leaf_func = $func;   # keep reference to leaf
    die "unknown function: $_" unless defined $func;
    my $func_id = $func->{id};
    unless ($node->{children}->{$func_id}) {
      $node->{children}->{$func_id} = {
        function => $func,
        calls => 0,
        total_time => 0.0,
        own_time => 0.0,
        children => {},
      };
    }
    $node = $node->{children}->{$func_id};
  }
  $node->{own_time} += $time;
  $node->{calls} += $calls;
  $leaf_func->{exclusive_time} += $time;
  $leaf_func->{call_count} += $calls;
}

die "too many roots" unless scalar keys %{$call_tree->{children}} == 1;
$call_tree = (values %{$call_tree->{children}})[0];

printf "Tundra Lua Profile Report\n";
printf "(%d functions, %d invocation chains)\n", scalar keys %functions, $chain_count;

printf "\nFlat profile of the top 50 functions by exclusive time\n\n";

do {
  my @top_funcs = sort { $b->{exclusive_time} <=> $a->{exclusive_time} } values %functions;
  my $count = 0;
  printf "%-40s %7s %10s    %s\n", "Function", "Calls", "Time (ms)", "Location";
  foreach (@top_funcs) {
    printf "%-40s %7d %10.3f %s\n", $_->{name}, $_->{call_count}, $_->{exclusive_time}, $_->{loc};
    last if $count++ == 50;
  }
};


# Propgate times to compute inclusive totals for all nodes.
do {
  sub ChildTime {
    my $node = shift;
    if (not exists $node->{child_time}) {
      my $t = 0;
      while (my ($key, $child) = each %{$node->{children}}) {
        $t += $child->{own_time};
        $t += ChildTime($child);
      }
      $node->{child_time} = $t;
    }
    return $node->{child_time};
  }

  sub VisitCT {
    my $node = shift;
    $node->{inc_time} = $node->{own_time} + ChildTime($node);
    while (my ($k, $child) = each %{$node->{children}}) {
      VisitCT($child);
    }
  }

  VisitCT($call_tree);
};


my $max_level = 0;

do {
  # Create report data
  my @r;
  sub VisitLB {
    my $node = shift || die;
    my $level = shift || 0;
    $max_level = $level if $level > $max_level;
    
    my $name = $node->{function}->{name};
    my $label = (' ' x $level) . $name;
    push @r, {
      label => $label,
      level=> $level,
      node => $node,
      inc_time => $node->{inc_time},
      own_time => $node->{own_time},
    };

    my @children = values %{$node->{children}};
    @children = sort { $b->{inc_time} <=> $a->{inc_time} } @children;
    foreach (@children) {
      VisitLB($_, $level + 1);
    }
  }

  VisitLB($call_tree, 0);

  my $max_label = 0;
  foreach (@r) {
    if (length $_->{label} > $max_label) {
      $max_label = length $_->{label};
    }
  }

  printf "\n\nComplete call tree (by inclusive time)\n\n";

  my $title = "Function" . (' ' x ($max_label - 8));
  printf "%s-+------------+------------|%s\n", '-' x length $title, '-' x 20;
  printf "%s |   Inc (ms) |   Exc (ms) | Location\n", $title;
  printf "%s-+------------+------------|%s\n", '-' x length $title, '-' x 20;

  foreach (@r) {
    my $label = $_->{label} . ' ' x ($max_label - length $_->{label});
    my $node = $_->{node};
    my $fun = $node->{function};
    printf "%s | %10.3f | %10.3f | %s\n", $label, $node->{inc_time}, $node->{own_time}, $fun->{loc};
  }
};

