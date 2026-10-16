use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/..";
use Random;
my ($seed, $count) = @ARGV;
my $rng = Random->new($seed, "files");
my @edges = (
    'first last',
    'A Z',
    '0 0',
);
$rng->emit($count, \@edges, sub {
my @alphabet = split //, 'ABCabc019_-';
my @words;
for (1..2) {
    my $size = $rng->integer(1,24);
    push @words, join '', map { $alphabet[$rng->integer(0,$#alphabet)] } 1 .. $size;
}
return join ' ', @words;
});
