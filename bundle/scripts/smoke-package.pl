#!/usr/bin/env perl
# Check a portable desktop package without relying on an installed toolchain.
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Basename qw(dirname);
use File::Find ();
use File::Spec;
use File::Temp qw(tempdir);
use IPC::Open3 qw(open3);
use POSIX qw(WNOHANG);
use Time::HiRes qw(sleep time);

my $IS_WINDOWS = $^O eq 'MSWin32';

sub write_file {
    my ($path, $layer, $data) = @_;
    open my $fh, ">$layer", $path or die "cannot write $path: $!\n";
    print {$fh} $data;
    close $fh or die "cannot write $path: $!\n";
}

sub read_file {
    my ($path) = @_;
    open my $fh, '<:raw', $path or die "cannot read $path: $!\n";
    my $data = do { local $/; <$fh> };
    close $fh;
    return defined($data) ? $data : '';
}

# Reap a child within $seconds, killing it on timeout. Returns its exit code.
sub finish {
    my ($pid, $seconds, $what) = @_;
    my $deadline = time() + $seconds;
    while (waitpid($pid, WNOHANG) == 0) {
        if (time() >= $deadline) {
            kill 'KILL', $pid;
            waitpid($pid, 0);
            die "$what timed out\n";
        }
        sleep 0.1;
    }
    return $? >> 8;
}

my $root = abs_path($ARGV[0]);
die "Missing package path\n" unless defined $root;
# Children inherit %ENV; Win32 %ENV lookups ignore case.
$ENV{PATH} = $IS_WINDOWS
    ? join(';', File::Spec->catdir($ENV{SystemRoot}, 'System32'), $ENV{SystemRoot})
    : '/nonexistent';
# Children also inherit the cwd. Exit cleanup leaves it before removing it.
my $work = tempdir('cpsc bundle smoke XXXXXXXX', TMPDIR => 1, CLEANUP => 1);
chdir $work or die "cannot chdir to $work: $!\n";
my $compiler;
if ($IS_WINDOWS) {
    die "Expected a single portable .exe\n" unless -f $root && $root =~ /\.exe\z/;
    # Extraction happens before WebView startup and publishes a complete file by rename.
    my $desktop = system 1, $root;
    die "cannot start $root: $!\n" unless $desktop > 0;
    $compiler = File::Spec->catfile($work, 'cpsc.exe');
    my $extracted = eval {
        my $deadline = time() + 30;
        until (-f $compiler) {
            die "Desktop exited before extracting its compiler\n" if waitpid($desktop, WNOHANG) != 0;
            die "Compiler extraction timed out\n" if time() >= $deadline;
            sleep 0.1;
        }
        1;
    };
    my $error = $@;
    kill 'KILL', $desktop if waitpid($desktop, WNOHANG) == 0;
    finish($desktop, 10, 'Desktop shutdown');
    die $error unless $extracted;
} else {
    my @compilers;
    File::Find::find(sub { push @compilers, $File::Find::name if $_ eq 'cpsc' }, $root);
    die "Expected one packaged compiler, found [@compilers]\n" unless @compilers == 1;
    $compiler = $compilers[0];
    my $directory = dirname($compiler);
    die "Missing desktop executable\n" unless -f File::Spec->catfile($directory, 'cpsc-bundle');
    my $resources = $^O eq 'darwin' ? File::Spec->catdir(dirname($directory), 'Resources') : $directory;
    for my $filename ('LICENSE', 'ThirdPartyNotices.md', 'licenses/vscode.txt', 'licenses/plugin.md') {
        die "Missing $filename\n" unless -f File::Spec->catfile($resources, $filename);
    }
}
# Text mode: Windows sources and data files use CRLF.
write_file('data.txt', '', "relative file\n");
my $source = File::Spec->catfile($work, 'source file.cps');
write_file($source, '', qq{DECLARE N : INTEGER\nINPUT N\nOUTPUT N\nDECLARE S : STRING\nOPENFILE "data.txt" FOR READ\nREADFILE "data.txt", S\nCLOSEFILE "data.txt"\nOUTPUT S\n});
# Files instead of pipes: Windows cannot poll pipes, and files cannot fill up.
write_file('stdin', ':raw', "17\n");
open my $in, '<:raw', 'stdin' or die "cannot read stdin: $!\n";
open my $out, '>:raw', 'stdout' or die "cannot write stdout: $!\n";
open my $err, '>:raw', 'stderr' or die "cannot write stderr: $!\n";
my $pid = open3('<&' . fileno($in), '>&' . fileno($out), '>&' . fileno($err), $compiler, $source, '-r');
my $status = finish($pid, 30, 'Compiler run');
close $out;
close $err;
my $stdout = read_file('stdout');
my $stderr = read_file('stderr');
die "Compiler exited with $status: '$stderr'\n" unless $status == 0;
(my $normalized = $stdout) =~ s/\r\n/\n/g;
die "Unexpected output: '$stdout'\n" unless $normalized eq "17\nrelative file\n";
die "Unexpected diagnostics: '$stderr'\n" unless $stderr eq '';
print "Desktop package smoke passed: portable application, compiler, stdin and relative files.\n";
