package Random;
use strict;
use warnings;
use Digest::SHA qw(sha256);

# Fixed byte order and decimal counter: independent of Perl's rand and hash seed.
sub new {
    my ($class, $seed, $group) = @_;
    return bless { seed => $seed, group => $group, counter => 0 }, $class;
}
sub integer {
    my ($self, $low, $high) = @_;
    die "invalid random range\n" if $high < $low || $high - $low >= 4294967296;
    my $bytes = sha256($self->{seed} . "\0" . $self->{group} . "\0" . $self->{counter}++);
    return $low + unpack('N', substr($bytes, 0, 4)) % ($high - $low + 1);
}
sub emit {
    my ($self, $count, $edges, $make) = @_;
    die "positive count required\n" unless defined($count) && $count =~ /^[1-9][0-9]*$/;
    binmode STDOUT; # Identical input bytes on every platform.
    print scalar(@$edges) + $count, "\n";
    print "$_\n" for @$edges;
    print $make->(), "\n" for 1 .. $count;
}
1;
