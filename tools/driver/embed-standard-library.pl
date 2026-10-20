#!/usr/bin/env perl
use strict;
use warnings;
use File::Basename qw(basename);

my ($output, @inputs) = @ARGV;
open my $out, '>', $output or die "$output: $!\n";
print {$out} "// Generated from pseudocode-lib; do not edit.\n";
for my $index (0 .. $#inputs) {
    open my $in, '<:raw', $inputs[$index] or die "$inputs[$index]: $!\n";
    local $/;
    my $data = <$in>;
    close $in or die "$inputs[$index]: $!\n";
    print {$out} "static const char LibraryData$index\[\] =\n";
    while (length $data) {
        my $chunk = substr($data, 0, 32, '');
        print {$out} '"', join('', map { sprintf '\\%03o', $_ } unpack 'C*', $chunk), "\"\n";
    }
    print {$out} ";\n";
}
print {$out} "static const LibraryFile LibraryFiles[] = {\n";
for my $index (0 .. $#inputs) {
    my $name = basename($inputs[$index]);
    die "Invalid library filename: $name\n" unless $name =~ /^[A-Za-z0-9_.-]+$/;
    print {$out} "    {\"$name\", LibraryData$index, sizeof(LibraryData$index) - 1},\n";
}
print {$out} "};\n";
close $out or die "$output: $!\n";
