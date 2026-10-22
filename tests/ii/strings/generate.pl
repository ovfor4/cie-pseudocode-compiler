use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/..";
use Random;
my ($seed, $count) = @ARGV;
my $rng = Random->new($seed, "strings");
my @edges = (
    '1 1 0 placeholder',
    '0 1 1 A',
    '0 4 1 Ab09',
    '0 1 4 Ab09',
    '0 2 0 Ab09',
);
$rng->emit($count, \@edges, sub {
my $size = $rng->integer(0,24);
return '1 1 0 placeholder' if $size == 0;
my @alphabet = split //, 'ABCXYZabcxyz019@_-';
my $text = join '', map { $alphabet[$rng->integer(0,$#alphabet)] } 1 .. $size;
my $start = $rng->integer(1,$size);
my $width = $rng->integer(0,$size-$start+1);
return "0 $start $width $text";
});
