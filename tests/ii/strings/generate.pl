use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/..";
use Random;
my ($seed, $count) = @ARGV;
my $rng = Random->new($seed, "strings");
my @edges = (
    '1 placeholder 1 0',
    '0 A 1 1',
    '0 Ab09 4 1',
    '0 Ab09 1 4',
    '0 Ab09 2 0',
);
$rng->emit($count, \@edges, sub {
my $size = $rng->integer(0,24);
return '1 placeholder 1 0' if $size == 0;
my @alphabet = split //, 'ABCXYZabcxyz019@_-';
my $text = join '', map { $alphabet[$rng->integer(0,$#alphabet)] } 1 .. $size;
my $start = $rng->integer(1,$size);
my $width = $rng->integer(0,$size-$start+1);
return "0 $text $start $width";
});
