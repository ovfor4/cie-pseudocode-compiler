use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/..";
use Random;
my ($seed, $count) = @ARGV;
my $rng = Random->new($seed, "calls");
my @edges = (
    '0 0 0',
    '1 -1000000 1000000',
    '10 1000000 -1000000',
);
$rng->emit($count, \@edges, sub {
return join ' ', $rng->integer(0,10), $rng->integer(-1000000,1000000),
                 $rng->integer(-1000000,1000000);
});
