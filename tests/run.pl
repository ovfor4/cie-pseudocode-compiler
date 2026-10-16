#!/usr/bin/env perl
# Run one compiler fixture. Only CRLF is normalized; failures are never waived.
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Copy ();
use File::Find ();
use File::Path qw(make_path remove_tree);
use File::Spec;
use FindBin;
use Getopt::Long qw(GetOptions);
use JSON::PP ();

$| = 1;

my $ROOT = abs_path(File::Spec->catdir($FindBin::Bin, File::Spec->updir()));
my $IS_WINDOWS = ($^O =~ /^(?:MSWin32|msys|cygwin)$/i) ? 1 : 0;

sub normalized {
    my ($data) = @_;
    $data = '' unless defined $data;
    my $copy = $data;
    $copy =~ s/\r\n/\n/g;
    return $copy;
}

# Single-quoted escaping: keeps trailing spaces, missing newlines and
# non-UTF-8 bytes visible without any non-core module.
sub show_bytes {
    my ($data) = @_;
    $data = '' unless defined $data;
    $data =~ s/\\/\\\\/g;
    $data =~ s/\n/\\n/g;
    $data =~ s/\r/\\r/g;
    $data =~ s/\t/\\t/g;
    $data =~ s/([^ -~])/sprintf('\\x{%02x}', ord($1))/ge;
    return "'$data'";
}

sub split_keeplines {
    my ($data) = @_;
    return () if !defined($data) || $data eq '';
    return ($data =~ /(?:[^\n]*\n|[^\n]+\z)/g);
}

# Like Python's bytes.splitlines(): exactly one trailing empty is dropped.
sub split_records {
    my ($data) = @_;
    return () if !defined($data) || $data eq '';
    my @records = split(/\n/, $data, -1);
    pop(@records) if $data =~ /\n\z/;
    return @records;
}

sub compare {
    my ($expected, $actual, $label) = @_;
    my $want = normalized($expected);
    my $got = normalized($actual);
    return if $want eq $got;
    my @left = split_keeplines($want);
    my @right = split_keeplines($got);
    my $count = @left < @right ? @left : @right;
    my $index = $count;
    for my $i (0 .. $count - 1) {
        if ($left[$i] ne $right[$i]) {
            $index = $i;
            last;
        }
    }
    my $line = $index + 1;
    my $want_show = $index < @left ? show_bytes($left[$index]) : '<missing>';
    my $got_show = $index < @right ? show_bytes($right[$index]) : '<missing>';
    die "$label differs at line $line: expected $want_show, actual $got_show\n";
}

sub check_result {
    my ($result, $expected, $exit_code) = @_;
    $exit_code = 0 unless defined $exit_code;
    my @errors;
    if ($result->{returncode} != $exit_code) {
        push @errors, "exit code: expected $exit_code, got $result->{returncode}";
    }
    if (length($result->{stderr})) {
        my $shown = show_bytes(substr($result->{stderr}, 0, 2000));
        push @errors, "unexpected stderr: $shown";
    }
    my $ok = eval { compare($expected, $result->{stdout}, 'stdout'); 1; };
    if (!$ok) {
        my $err = $@;
        chomp($err);
        push @errors, $err;
    }
    if (@errors) {
        die join("\n", @errors) . "\n";
    }
}

sub check_diagnostics {
    my ($result, $expectations) = @_;
    my @errors;
    if ($result->{returncode} != 1) {
        push @errors, "compiler exit code: expected 1, got $result->{returncode}";
    }
    if (length($result->{stdout})) {
        push @errors, 'unexpected stdout: ' . show_bytes(substr($result->{stdout}, 0, 1000));
    }
    # ASCII markers (error:, line N) are matched on raw bytes; the compiler
    # emits UTF-8 and ASCII patterns are unaffected by decoding.
    my @lines = split_records(normalized($result->{stderr}));
    for my $expectation (@{$expectations}) {
        my $reason = $expectation->{reason};
        my $line_no = $expectation->{line};
        my @matching = grep { /error:/ && /$reason/ } @lines;
        my $found = 0;
        for my $line (@matching) {
            if ($line =~ /\bline $line_no\b/) {
                $found = 1;
                last;
            }
        }
        if (!$found) {
            push @errors, 'missing error /' . $reason . '/ at line ' . $line_no
                . '; matching diagnostics: [' . join(', ', map { show_bytes($_) } @matching) . ']';
        }
    }
    my $terminated = grep { $_ eq 'compilation terminated.' } @lines;
    if ($terminated != 1) {
        push @errors, "expected exactly one 'compilation terminated.' line";
    }
    if (@errors) {
        die join("\n", @errors) . "\n";
    }
}

sub read_file {
    my ($path) = @_;
    open my $fh, '<:raw', $path or die "cannot read $path: $!\n";
    my $data = do { local $/; <$fh> };
    close $fh;
    return defined($data) ? $data : '';
}

sub write_file {
    my ($path, $data) = @_;
    $data = '' unless defined $data;
    open my $fh, '>:raw', $path or die "cannot write $path: $!\n";
    print {$fh} $data;
    close $fh;
}

sub commands_new {
    my ($directory) = @_;
    return {dir => $directory, index => 0};
}

sub write_metadata {
    my ($stem, $metadata) = @_;
    my $json = JSON::PP->new->canonical(1)->pretty(1)->encode($metadata);
    write_file($stem . '.json', $json);
}

# Run one command without a shell, retaining argv, cwd, input, status and
# output under the case result directory. Dies on timeout or launch failure.
sub run_command {
    my ($state, $command, $cwd, $data, $timeout) = @_;
    $data = '' unless defined $data;
    $timeout = 30 unless defined $timeout;
    $state->{index}++;
    my $stem = File::Spec->catfile($state->{dir}, sprintf('%02d', $state->{index}));
    my %metadata = (command => [@{$command}], cwd => "$cwd", timeout => $timeout + 0);
    write_file($stem . '.stdin', $data);
    my $pid = fork();
    if (!defined($pid)) {
        $metadata{launch_error} = "fork failed: $!";
        write_metadata($stem, \%metadata);
        die "cannot launch @{$command}: fork failed: $!\n";
    }
    if ($pid == 0) {
        open STDIN, '<:raw', $stem . '.stdin' or exit 126;
        open STDOUT, '>:raw', $stem . '.stdout' or exit 126;
        open STDERR, '>:raw', $stem . '.stderr' or exit 126;
        if (!chdir($cwd)) {
            print STDERR "CPSC_LAUNCH_ERROR: cannot chdir to $cwd: $!";
            exit 126;
        }
        { exec {$command->[0]} @{$command}; }
        print STDERR "CPSC_LAUNCH_ERROR: cannot exec @{$command}: $!";
        exit 126;
    }
    my $alarm_secs = int($timeout);
    $alarm_secs = 1 if $alarm_secs < 1;
    my $timed_out = 0;
    local $SIG{ALRM} = sub { $timed_out = 1; kill('KILL', $pid); };
    alarm($alarm_secs);
    waitpid($pid, 0);
    alarm(0);
    my $status = $?;
    if ($timed_out) {
        waitpid($pid, 0);
        for my $suffix ('.stdout', '.stderr') {
            if (!-f ($stem . $suffix)) {
                write_file($stem . $suffix, '');
            }
        }
        $metadata{timeout_expired} = JSON::PP::true;
        write_metadata($stem, \%metadata);
        die "command timed out after ${timeout}s: @{$command}\n";
    }
    my $exit_code = $status >> 8;
    my $out = -f ($stem . '.stdout') ? read_file($stem . '.stdout') : '';
    my $err = -f ($stem . '.stderr') ? read_file($stem . '.stderr') : '';
    if ($exit_code == 126 && $err =~ s/^CPSC_LAUNCH_ERROR: //) {
        chomp($err);
        $metadata{launch_error} = $err;
        write_metadata($stem, \%metadata);
        write_file($stem . '.stdout', $out);
        write_file($stem . '.stderr', $err);
        die "cannot launch @{$command}: $err\n";
    }
    $metadata{exit_code} = $exit_code + 0;
    write_metadata($stem, \%metadata);
    write_file($stem . '.stdout', $out);
    write_file($stem . '.stderr', $err);
    return {returncode => $exit_code + 0, stdout => $out, stderr => $err};
}

sub copy_tree {
    my ($src, $dst) = @_;
    opendir(my $dh, $src) or die "cannot read $src: $!\n";
    my @entries = sort grep { $_ ne '.' && $_ ne '..' } readdir($dh);
    closedir($dh);
    for my $entry (@entries) {
        my $from = File::Spec->catfile($src, $entry);
        my $to = File::Spec->catfile($dst, $entry);
        if (-d $from && !-l $from) {
            make_path($to);
            copy_tree($from, $to);
        }
        elsif (-f $from || -l $from) {
            File::Copy::copy($from, $to) or die "cannot copy $from: $!\n";
        }
    }
}

sub workspace {
    my ($case_dir, $directory, $name) = @_;
    my $work = File::Spec->catdir($directory, $name);
    make_path($work);
    my $files = File::Spec->catdir($case_dir, 'files');
    if (-d $files) {
        copy_tree($files, $work);
    }
    return $work;
}

sub check_files {
    my ($case_dir, $work) = @_;
    my $expected_dir = File::Spec->catdir($case_dir, 'expected-files');
    return unless -d $expected_dir;
    my @found;
    File::Find::find(
        {wanted => sub { push(@found, $File::Find::name) if -f $_; }, no_chdir => 1},
        $expected_dir,
    );
    for my $expected (sort @found) {
        my $relative = File::Spec->abs2rel($expected, $expected_dir);
        my $actual = File::Spec->catfile($work, File::Spec->splitdir($relative));
        if (!-f $actual) {
            die "missing output file: $relative\n";
        }
        compare(read_file($expected), read_file($actual), $relative);
    }
}

sub main {
    my @args = @_;
    local @ARGV = @args;
    my ($opt_case, $opt_cpsc, $opt_run_mode, $opt_reference, $opt_perl, $opt_seed,
        $opt_count, $opt_results);
    $opt_perl = 'perl';
    $opt_seed = '';
    $opt_count = 10000;
    $opt_results = File::Spec->catdir($ROOT, 'build', 'test-results');
    my $parsed = GetOptions(
        'case=s'      => \$opt_case,
        'cpsc=s'      => \$opt_cpsc,
        'run-mode=s'  => \$opt_run_mode,
        'reference=s' => \$opt_reference,
        'perl=s'      => \$opt_perl,
        'seed=s'      => \$opt_seed,
        'count=i'     => \$opt_count,
        'results=s'   => \$opt_results,
    );
    if (!$parsed || !defined($opt_case) || !defined($opt_cpsc) || !defined($opt_run_mode)) {
        warn "usage: $0 --case MANIFEST --cpsc CPSC --run-mode MODE [options]\n";
        return 2;
    }
    if ($opt_run_mode !~ /^(?:EMBEDDED|PATH|NONE)$/) {
        warn "invalid --run-mode: $opt_run_mode\n";
        return 2;
    }
    if ($opt_count < 1) {
        warn "--count must be positive\n";
        return 2;
    }
    my $case_abs = abs_path($opt_case);
    my $cpsc_abs = abs_path($opt_cpsc);
    my $reference_abs = defined($opt_reference) ? abs_path($opt_reference) : undef;
    my $case_dir = File::Spec->catdir((File::Spec->splitpath($case_abs))[0, 1]);
    $case_dir =~ s{[\\/]$}{};
    my @parts = File::Spec->splitdir($case_dir);
    my $suite = $parts[-2];
    my $name = $parts[-1];
    open my $cfg_fh, '<:raw', $case_abs or die "cannot read $case_abs: $!\n";
    my $config_text = do { local $/; <$cfg_fh> };
    close $cfg_fh;
    my $config = JSON::PP->new->decode($config_text);
    open my $git_fh, '-|', 'git', '-C', $ROOT, 'rev-parse', 'HEAD'
        or die "cannot run git: $!\n";
    my $commit = <$git_fh>;
    close $git_fh;
    if ($? != 0 || !defined($commit)) {
        die "cannot determine HEAD commit\n";
    }
    chomp($commit);
    my $seed = length($opt_seed) ? $opt_seed : $commit;
    my $directory = File::Spec->catdir(abs_path($opt_results), $suite, $name);
    if (-e $directory) {
        remove_tree($directory);
    }
    make_path($directory);
    my $commands = commands_new($directory);
    my $report = {
        case         => "$suite/$name",
        commit       => $commit,
        seed         => $seed,
        random_count => $opt_count + 0,
        platform    => "$^O",
        run_mode     => $opt_run_mode,
        failures     => [],
    };
    my $source = File::Spec->catfile($case_dir, 'program.cps');

    my $attempt = sub {
        my ($label, $code) = @_;
        my $ok = eval { $code->(); 1; };
        if (!$ok) {
            my $err = "$@";
            chomp($err);
            push(@{$report->{failures}}, {stage => $label, message => $err});
            print "FAIL $suite/$name [$label]: $err\n";
        }
        else {
            print "PASS $suite/$name [$label]\n";
        }
    };

    my $diagnostics = sub {
        for my $output_format ('native', 'ir') {
            my $work = workspace($case_dir, $directory, $output_format);
            my $target = File::Spec->catfile($work,
                $IS_WINDOWS ? 'program.exe' : 'program.bin');
            my @flags = $output_format eq 'ir' ? ('-S') : ();
            for my $existing (0, 1) {
                my $sentinel = 'existing output must survive' . "\n";
                if ($existing) {
                    write_file($target, $sentinel);
                }
                my $result = run_command($commands,
                    [$cpsc_abs, $source, @flags, '-o', $target], $work, '', 120);
                my $preservation_error;
                if ($existing && (!-f $target || read_file($target) ne $sentinel)) {
                    $preservation_error = 'compilation failure overwrote existing output';
                }
                if (!$existing && -f $target) {
                    $preservation_error = 'compilation failure created output';
                    unlink($target);
                }
                my $captured_result = $result;
                my $captured_error = $preservation_error;
                $attempt->(
                    "$output_format/" . ($existing ? 'existing' : 'absent'),
                    sub {
                        if (defined($captured_error)) {
                            die "$captured_error\n";
                        }
                        check_diagnostics($captured_result, $config->{errors});
                    },
                );
            }
        }
    };

    my $setup_ok = eval {
        if ($suite eq 'iii') {
            $diagnostics->();
        }
        else {
            my $input_path = File::Spec->catfile($case_dir, 'input.txt');
            my $data = -f $input_path ? read_file($input_path) : '';
            my @records;
            my $expected;
            if ($suite eq 'ii') {
                if (!defined($reference_abs)) {
                    die "differential tests require --reference\n";
                }
                my $generated = run_command($commands,
                    [$opt_perl, File::Spec->catfile($case_dir, 'generate.pl'), $seed, "$opt_count"],
                    $directory, '', 30);
                if ($generated->{returncode} != 0 || length($generated->{stderr})) {
                    die 'generator failed: ' . show_bytes($generated->{stderr}) . "\n";
                }
                $data = $generated->{stdout};
                @records = split_records(normalized($data));
                if (!@records || $records[0] !~ /^\d+$/ || $records[0] + 0 != @records - 1) {
                    die "generator must emit record count followed by one record per line\n";
                }
                my $work = workspace($case_dir, $directory, 'reference');
                my $exec_timeout = exists($config->{timeout}) ? $config->{timeout} + 0 : 30;
                my $reference = run_command($commands, [$reference_abs], $work, $data,
                    $exec_timeout);
                if ($reference->{returncode} != 0 || length($reference->{stderr})) {
                    die "reference failed ($reference->{returncode}): "
                        . show_bytes($reference->{stderr}) . "\n";
                }
                $expected = $reference->{stdout};
                my @expected_lines = split_records(normalized($expected));
                if (@expected_lines != @records - 1) {
                    die "reference must emit exactly one output line per input record\n";
                }
            }
            else {
                $expected = read_file(File::Spec->catfile($case_dir, 'expected.txt'));
            }
            write_file(File::Spec->catfile($directory, 'input.txt'), $data);
            write_file(File::Spec->catfile($directory, 'expected.txt'), $expected);

            my $execute = sub {
                my ($mode) = @_;
                my $work = workspace($case_dir, $directory, $mode);
                my $command;
                if ($mode eq 'native') {
                    my $target = File::Spec->catfile($work,
                        $IS_WINDOWS ? 'program.exe' : 'program.bin');
                    my $compiled = run_command($commands,
                        [$cpsc_abs, $source, '-o', $target], $work, '', 120);
                    check_result($compiled, '');
                    $command = [$target];
                }
                else {
                    $command = [$cpsc_abs, $source, '-r'];
                }
                my $exec_timeout = exists($config->{timeout}) ? $config->{timeout} + 0 : 30;
                my $result = run_command($commands, $command, $work, $data, $exec_timeout);
                if ($suite eq 'ii' && normalized($result->{stdout}) ne normalized($expected)) {
                    my @wanted = split_keeplines(normalized($expected));
                    my @got = split_keeplines(normalized($result->{stdout}));
                    my $count = @wanted < @got ? @wanted : @got;
                    my $index = $count;
                    for my $i (0 .. $count - 1) {
                        if ($wanted[$i] ne $got[$i]) {
                            $index = $i;
                            last;
                        }
                    }
                    my $record = ($index + 1 < @records) ? $records[$index + 1] : '<extra output>';
                    print 'First differing record ' . ($index + 1) . ': ' . show_bytes($record) . "\n";
                    my $difference = {
                        record   => $index + 1,
                        input    => $record,
                        expected => $index < @wanted ? show_bytes($wanted[$index]) : '<missing>',
                        actual   => $index < @got ? show_bytes($got[$index]) : '<missing>',
                    };
                    my $diff_json = JSON::PP->new->canonical(1)->pretty(1)->encode($difference);
                    write_file(File::Spec->catfile($directory, "$mode-first-difference.json"),
                        $diff_json);
                }
                my $exit_code = exists($config->{exit_code}) ? $config->{exit_code} + 0 : 0;
                check_result($result, $expected, $exit_code);
                check_files($case_dir, $work);
            };
            my @modes = ('native');
            if ($opt_run_mode ne 'NONE') {
                push(@modes, 'runner');
            }
            for my $mode (@modes) {
                my $captured = $mode;
                $attempt->($captured, sub { $execute->($captured); });
            }
        }
        1;
    };
    if (!$setup_ok) {
        my $err = "$@";
        chomp($err);
        push(@{$report->{failures}}, {stage => 'setup', message => $err});
        print "FAIL $suite/$name [setup]: $err\n";
    }
    $report->{passed} = @{$report->{failures}} ? JSON::PP::false : JSON::PP::true;
    my $report_json = JSON::PP->new->canonical(1)->pretty(1)->encode($report);
    write_file(File::Spec->catfile($directory, 'result.json'), $report_json);
    print "Seed: $seed; results: $directory\n";
    return $report->{passed} ? 0 : 1;
}

exit(main(@ARGV)) unless caller;

1;
