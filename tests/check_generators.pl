#!/usr/bin/env perl
# Require identical generated input bytes across platforms and Perl versions.
use strict;
use warnings;
use Digest::SHA qw(sha256_hex);
use File::Spec;
use File::Temp ();
use FindBin;
use JSON::PP ();

die "usage: $0 <perl>\n" unless @ARGV == 1;
my ($perl) = @ARGV;

my $dir = File::Spec->catdir($FindBin::Bin, 'ii');
my $vectors_path = File::Spec->catfile($dir, 'generator-vectors.json');
open my $fh, '<:raw', $vectors_path or die "cannot read $vectors_path: $!\n";
my $vectors_text = do { local $/; <$fh> };
close $fh;
my $vectors = JSON::PP->new->decode($vectors_text);

for my $name (sort keys %{$vectors}) {
    my $digest = $vectors->{$name};
    my $generate = File::Spec->catfile($dir, $name, 'generate.pl');
    my @outputs;
    for my $seed (('0' x 40, '0' x 40, '1' x 40)) {
        my ($exit, $out, $err) = run_capture($perl, $generate, $seed, '100');
        die "$name: generator exited with $exit\n" if $exit != 0;
        if (length $err) {
            die "$name: unexpected stderr " . display($err) . "\n";
        }
        push @outputs, $out;
    }
    if ($outputs[0] ne $outputs[1] || $outputs[0] eq $outputs[2]) {
        die "$name: seed reproducibility failed\n";
    }
    if (sha256_hex($outputs[0]) ne $digest) {
        die "$name: generated input differs from the portable test vector\n";
    }
    print "PASS $name: repeatable, seed-sensitive, portable bytes\n";
}

sub run_capture {
    my (@command) = @_;
    my $out_fh = File::Temp->new(UNLINK => 1);
    my $err_fh = File::Temp->new(UNLINK => 1);
    my $out_path = $out_fh->filename;
    my $err_path = $err_fh->filename;
    close $out_fh;
    close $err_fh;
    my $pid = fork();
    die "cannot fork: $!\n" unless defined $pid;
    if ($pid == 0) {
        open STDOUT, '>', $out_path or exit 127;
        open STDERR, '>', $err_path or exit 127;
        binmode STDOUT;
        binmode STDERR;
        exec { $command[0] } @command;
        exit 127;
    }
    my $timed_out = 0;
    local $SIG{ALRM} = sub { $timed_out = 1; kill 'KILL', $pid; };
    alarm 10;
    waitpid($pid, 0);
    alarm 0;
    if ($timed_out) {
        waitpid($pid, 0);
        die "generator timed out after 10s: @command\n";
    }
    my $status = $?;
    open my $out_in, '<:raw', $out_path or die "cannot read $out_path: $!\n";
    my $out = do { local $/; <$out_in> };
    close $out_in;
    open my $err_in, '<:raw', $err_path or die "cannot read $err_path: $!\n";
    my $err = do { local $/; <$err_in> };
    close $err_in;
    $out = '' unless defined $out;
    $err = '' unless defined $err;
    my $exit = ($status >> 8);
    return ($exit, $out, $err);
}

sub display {
    my ($bytes) = @_;
    $bytes =~ s/([^ -~\n])/sprintf('\\x{%02x}', ord($1))/ge;
    $bytes =~ s/\n/\\n/g;
    return "'$bytes'";
}
