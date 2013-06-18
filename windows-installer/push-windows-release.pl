#! /usr/bin/env perl

use strict;
use warnings;
use File::Slurp;
use Digest::MD5;

my $tag = shift or die "Need a tag";

print "Building windows release of Tundra with tag \"$tag\":\n";
print " - Tundra-Setup-$tag.exe\n";
print " - Tundra-Binaries-$tag.zip\n";

system "make CROSSMINGW=yes installer windows-zip";
die "build failed" unless $? == 0;

for my $f (qw(Tundra-Setup.exe Tundra-Binaries.zip)) {
  die "build.mingw/$f missing" unless -f "build.mingw/$f";
}

my @server_names = ();
my @md5s = ();

for my $f (qw(Tundra-Setup.exe Tundra-Binaries.zip)) {
  my $mod_name = $f;
  $mod_name =~ s/^(.*)\.([^.]*)$/$1-$tag.$2/;
  push @server_names, $mod_name;
  open my $fh, '<', "build.mingw/$f" or die;
  binmode $fh;
  my $context = Digest::MD5->new;
  $context->addfile($fh);
  close $fh;
  push @md5s, $context->hexdigest;
  system "s3cmd put --acl-public build.mingw/$f s3://tundra2-builds/$mod_name\n";
  die "failed to upload $f" unless $? == 0;
}

my $base_uri = 'http://tundra2-builds.s3.amazonaws.com';
my $readme = read_file 'README.md';
my @now = localtime;
my $date = sprintf "%04d-%02d-%02d", $now[5] + 1900, $now[4] + 1, $now[3];
my $note =<<TEXT
- 2.0 $tag ($date)
  - [installer]($base_uri/$server_names[0]) MD5: \`$md5s[0]\`
  - [zip]($base_uri/$server_names[1]) MD5: \`$md5s[1]\`
TEXT
;

$readme =~ s/(Windows installers are available for download:\s*)/$1$note/;

open my $fh, '>', 'README.md';
print $fh $readme;
close $fh;
