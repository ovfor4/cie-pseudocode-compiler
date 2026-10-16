use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/..";
use Random;
my ($seed, $count) = @ARGV;
my $rng = Random->new($seed, "arithmetic");
my @edges = (
    '9223372036854775807 1',
    '-9223372036854775808 1',
    '-9223372036854775808 2',
    '0 -1',
    '7 -3',
    '-7 3',
    '-7 -3',
    '1 1',
);
$rng->emit($count, \@edges, sub {
my $a = $rng->integer(-1000000, 1000000);
my $b = $rng->integer(-1000000, 1000000);
$b = 1 if $b == 0;
return "$a $b";
});
