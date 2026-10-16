#!/usr/bin/env perl
# Summarize retained case results locally and in GitHub Actions.
use strict;
use warnings;
use File::Path qw(make_path);
use File::Spec;
use JSON::PP ();

die "usage: $0 <directory>\n" unless @ARGV == 1;
my ($directory) = @ARGV;

my $summary = summarize($directory);
print $summary;
make_path($directory);
my $summary_path = File::Spec->catfile($directory, 'summary.md');
open my $out, '>:raw', $summary_path or die "cannot write $summary_path: $!\n";
print {$out} $summary;
close $out;
if (defined $ENV{GITHUB_STEP_SUMMARY} && length $ENV{GITHUB_STEP_SUMMARY}) {
    open my $step, '>>:raw', $ENV{GITHUB_STEP_SUMMARY}
        or die "cannot append to \$GITHUB_STEP_SUMMARY: $!\n";
    print {$step} $summary;
    close $step;
}

sub summarize {
    my ($dir) = @_;
    my @paths = sort glob(File::Spec->catfile($dir, '*', '*', 'result.json'));
    my @results;
    for my $path (@paths) {
        next unless -f $path;
        open my $fh, '<:raw', $path or die "cannot read $path: $!\n";
        my $text = do { local $/; <$fh> };
        close $fh;
        push @results, JSON::PP->new->decode($text);
    }
    if (!@results) {
        return "No compiler case results were produced; inspect the build/CTest log.\n";
    }
    my @failures = grep { !$_->{passed} } @results;
    my @lines = (
        sprintf('Compiler cases: %d/%d passed.', scalar(@results) - scalar(@failures), scalar(@results)),
        '',
        '| Group | Passed | Total |',
        '|---|---:|---:|',
    );
    for my $suite (qw(i ii iii)) {
        my @subset = grep { $_->{case} =~ m{^\Q$suite\E/} } @results;
        my $passed = grep { $_->{passed} } @subset;
        push @lines, sprintf('| %s | %d | %d |', $suite, $passed, scalar(@subset));
    }
    push @lines, '',
        sprintf('Commit: `%s`. Seed: `%s`.', $results[0]{commit}, $results[0]{seed}),
        'Fixed expectations: pending human review.';
    if (@failures) {
        push @lines, '', 'Failures:', '';
        for my $result (@failures) {
            my $stages = join(', ', map { $_->{stage} } @{$result->{failures}});
            push @lines, sprintf('- `%s` (%s); see its `result.json` and command logs.',
                $result->{case}, $stages);
        }
    }
    return join("\n", @lines) . "\n";
}
