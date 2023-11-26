use strict;

my $out_fn = shift;

my $sha = 'unknown';
my $branch = 'unknown';

if ($ENV{'GITHUB_SHA'}) {
    $sha = $ENV{'GITHUB_SHA'};
    $branch = 'releases';
} elsif (-d '.git') {
    my @lines = `git branch --no-color`;
    for (@lines) {
        $branch = $1 if /^\* (.*)/;
    }

    my $ref_fn = ".git/refs/heads/$branch";
    if (-f "$ref_fn") {
        open my $inf, "<", $ref_fn or die;
        $sha = <$inf>;
        chomp $sha;
        close $inf;
    }
}

my $text = <<EOF
const char g_GitVersion[] = "$sha";
const char g_GitBranch[] = "$branch";
EOF
;

my $need_update = 1;

if (-f $out_fn) {
    open my $inf, "<", $out_fn;
    local $/ = undef;
    my $data = <$inf>;
    close $inf;
    $need_update = 0 if $data eq $text;
}

if ($need_update) {
    print "Updating git version stamp. SHA: $sha, branch: $branch\n";

    open my $outf, ">", $out_fn;
    print $outf $text;
    close $outf;
}

