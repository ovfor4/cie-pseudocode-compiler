use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/..";
use Random;
my ($seed, $count) = @ARGV;
my $rng = Random->new($seed, "arrays");
my @edges = (
    '-20 1 0 0',
    '0 12 -100 1000',
    '1 2 1 -1',
    '-5 12 100 -1000',
);
$rng->emit($count, \@edges, sub {
return join ' ', $rng->integer(-20,20), $rng->integer(1,12),
                 $rng->integer(-100,100), $rng->integer(-1000,1000);
});
