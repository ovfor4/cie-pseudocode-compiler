use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/..";
use Random;
my ($seed, $count) = @ARGV;
my $rng = Random->new($seed, "overload-methods");
my @edges = (
    '0 0 1 x',
    '-1000 1000 8 boundary',
    '1000 -1000 8 z',
    '0 24 2 equal',
    '0 23 2 below',
    '0 25 2 above',
);
$rng->emit($count, \@edges, sub {
    return join ' ', $rng->integer(-1000,1000), $rng->integer(-1000,1000),
                    $rng->integer(1,8), 'x' x $rng->integer(1,24);
});
