use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/..";
use Random;
my ($seed, $count) = @ARGV;
my $rng = Random->new($seed, "native-types");
my @edges = (
    '0 0',
    '2147483647 127',
    '2147483648 128',
    '4294967295 255',
    '4294967296 0',
    '9223372036854775807 255',
    '-9223372036854775808 128',
    '-1 255',
);
$rng->emit($count, \@edges, sub {
return join ' ', $rng->integer(-1000000,1000000), $rng->integer(0,255);
});
