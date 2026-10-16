use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/..";
use Random;
my ($seed, $count) = @ARGV;
my $rng = Random->new($seed, "real");
my @edges = (
    '0 0',
    '-8192 8192',
    '8192 -8192',
    '1 1',
    '-1 1',
);
$rng->emit($count, \@edges, sub {
return join ' ', $rng->integer(-8192,8192), $rng->integer(-8192,8192);
});
