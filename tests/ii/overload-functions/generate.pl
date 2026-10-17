use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/..";
use Random;
my ($seed, $count) = @ARGV;
my $rng = Random->new($seed, "overload-functions");
my @edges = (
    '0 0 1 0 x',
    '-1000 1000 8 -8 boundary',
    '1000 -1000 8 8 z',
    '-7 2 8 -3 cancellation',
    '1 -2 2 0 pair',
    '-1 2 2 -1 positive',
);
$rng->emit($count, \@edges, sub {
    return join ' ', $rng->integer(-1000,1000), $rng->integer(-1000,1000),
                    $rng->integer(1,8), $rng->integer(-8,8),
                    'x' x $rng->integer(1,24);
});
