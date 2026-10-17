#!/usr/bin/env perl
# Small contract checks for the test harness, including intentional failures.
use strict;
use warnings;
use File::Spec;
use File::Temp qw(tempdir);
use FindBin;
use JSON::PP ();
use Test::More;

require File::Spec->catfile($FindBin::Bin, 'run.pl');

eval { compare("a\nb\n", "a\r\nb\r\n", 'stdout'); };
ok(!$@, 'CRLF-only difference is accepted');
for my $bad ("a\nb", "a \nb\n", "A\nb\n", "a\n\nb\n") {
    my $ok = eval { compare("a\nb\n", $bad, 'stdout'); 1; };
    ok(!$ok, 'strict output rejects ' . show_bytes($bad));
}

for my $case ([1, ''], [-11, ''], [0, 'unexpected']) {
    my ($code, $stderr) = @{$case};
    my $ok = eval {
        check_result({returncode => $code, stdout => "ok\n", stderr => $stderr}, "ok\n");
        1;
    };
    ok(!$ok, "exit code $code with stderr " . show_bytes($stderr) . ' fails');
}

my $good = {
    returncode => 1,
    stdout     => '',
    stderr     => "error: Expected ':' at line 3\ncompilation terminated.\n",
};
my $accepted = eval {
    check_diagnostics($good, [{reason => "Expected ':'", line => 3}]);
    1;
};
ok($accepted, 'matching reason and location accepted');
for my $bad (
    "error: Expected ':' at line 4",
    "error: Expected ':'",
    "error: Expected ':' at line 30\nerror: unrelated at line 3",
    )
{
    my $ok = eval {
        check_diagnostics(
            {returncode => 1, stdout => '', stderr => $bad . "\ncompilation terminated.\n"},
            [{reason => "Expected ':'", line => 3}],
        );
        1;
    };
    ok(!$ok, 'diagnostic mismatch rejected: ' . show_bytes($bad));
}

my $file_expectation = [{reason => "Expected ':'", line => 3, file => '/fixtures/lib.cps'}];
for my $sample (
    ["error: Expected ':' at line 3 in /fixtures/lib.cps", 1],
    ["error: Expected ':' at line 3 in /fixtures/program.cps", 0],
    ["error: Expected ':' at line 3 in /fixtures/lib.cps.extra", 0],
    ["error: Expected ':' at line 30 in /fixtures/lib.cps", 0],
    ["error: Expected ':' at line 3\nerror: unrelated at line 3 in /fixtures/lib.cps", 0],
) {
    my ($diagnostic, $expected) = @{$sample};
    my $ok = eval {
        check_diagnostics({returncode => 1, stdout => '',
            stderr => $diagnostic . "\ncompilation terminated.\n"}, $file_expectation);
        1;
    };
    is($ok ? 1 : 0, $expected, 'file-qualified diagnostic: ' . show_bytes($diagnostic));
}

my $dir = tempdir(CLEANUP => 1);
my $state = commands_new($dir);
my $ran = eval { run_command($state, [$^X, '-e', 'sleep 5'], $dir, '', 1); 1; };
my $timed_out = "$@";
ok(!$ran, 'overlong command fails');
like($timed_out, qr/timed out/, 'timeout message names the cause');
my $metadata = JSON::PP->new->decode(read_file(File::Spec->catfile($dir, '01.json')));
ok($metadata->{timeout_expired}, 'timeout recorded in command metadata');

done_testing();
