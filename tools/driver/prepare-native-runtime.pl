#!/usr/bin/env perl
# Freeze the build toolchain's native link inputs into the compiler.
use strict;
use warnings;
use Compress::Zlib qw(compress);
use Cwd qw(abs_path);
use File::Basename qw(basename dirname);
use File::Find qw(find);
use File::Path qw(make_path);
use File::Spec;
use File::Temp qw(tempdir);
use Getopt::Long qw(GetOptions);
use JSON::PP ();
use Text::ParseWords qw(shellwords);

my ($compiler, $output, $platform);
my @flags;
Getopt::Long::Configure('no_auto_abbrev', 'no_ignore_case');
GetOptions('compiler=s' => \$compiler, 'output=s' => \$output,
    'platform=s' => \$platform, 'flag=s@' => \@flags)
    or die "Invalid native runtime options\n";
die "Usage: $0 --compiler=PATH --output=PATH --platform=linux|windows|macos [--flag=VALUE ...]\n"
    if @ARGV || !defined($compiler) || !defined($output) || !defined($platform)
        || $platform !~ /\A(?:linux|windows|macos)\z/;
my $directory = dirname($output);
make_path($directory);
my $capture = tempdir(CLEANUP => 1);

sub read_file {
    my ($path) = @_;
    open my $fh, '<:raw', $path or die "Cannot read $path: $!\n";
    local $/;
    my $data = <$fh>;
    close $fh or die "Cannot close $path: $!\n";
    return defined($data) ? $data : '';
}

sub write_file {
    my ($path, $data) = @_;
    open my $fh, '>:raw', $path or die "Cannot write $path: $!\n";
    print {$fh} $data or die "Cannot write $path: $!\n";
    close $fh or die "Cannot close $path: $!\n";
}

sub compiler {
    my @command = ($compiler, @flags, @_);
    my $out_path = File::Spec->catfile($capture, 'stdout');
    my $err_path = File::Spec->catfile($capture, 'stderr');
    my $pid = fork();
    die "Cannot fork: $!\n" unless defined $pid;
    if ($pid == 0) {
        open STDOUT, '>:raw', $out_path or die "Cannot redirect stdout: $!\n";
        open STDERR, '>:raw', $err_path or die "Cannot redirect stderr: $!\n";
        { exec {$command[0]} @command; }
        die "Cannot execute $command[0]: $!\n";
    }
    waitpid($pid, 0) == $pid or die "Cannot wait for compiler: $!\n";
    my $status = $?;
    my ($stdout, $stderr) = (read_file($out_path), read_file($err_path));
    die "Compiler failed (status $status): @command\n$stderr" if $status != 0;
    s/\r\n/\n/g for ($stdout, $stderr);
    return ($stdout, $stderr);
}

# An actual object lets the driver validate its inputs without linking a probe.
my $source = File::Spec->catfile($directory, 'native-probe.cc');
my $obj = File::Spec->catfile($directory, 'native-probe.o');
my $binary = File::Spec->catfile($directory, 'native-probe.out');
write_file($source, "int main() { return 0; }\n");
compiler('-c', $source, '-o', $obj);
my @static = $platform eq 'macos' ? () : $platform eq 'windows'
    ? ('-static') : ('-static-libstdc++', '-static-libgcc');
my (undef, $trace) = compiler('-###', '-fuse-ld=lld', @static, $obj, '-o', $binary);
my @commands = map { [shellwords($_)] } grep { /^\s*"/ } split /\n/, $trace;
die "Expected one native linker command: $trace" unless @commands == 1;
my @command = @{$commands[0]};
shift @command;
my @directories = map { substr($_, 2) } grep { /^-L/ } @command;
my ($target) = compiler('-print-effective-triple');
$target =~ s/^\s+|\s+$//g;
my (%files, %scripts, %mapped);
my (@file_order, @mapped_order, @arguments);
my $root = '@CPSC_RUNTIME@';

sub resolve {
    my ($name, $static_only) = @_;
    my @names = $name =~ /^:/ ? (substr($name, 1))
        : map { "lib$name$_" } ($platform eq 'macos' ? ('.tbd', '.dylib', '.a')
            : $static_only ? ('.a') : ('.dll.a', '.so', '.a'));
    for my $dir (@directories) {
        for my $filename (@names) {
            my $path = File::Spec->catfile($dir, $filename);
            return $path if -f $path;
        }
    }
    for my $filename (@names) {
        my ($found) = compiler("-print-file-name=$filename");
        $found =~ s/^\s+|\s+$//g;
        return $found if -f $found;
    }
    die "Cannot find native link input $name\n";
}

sub unix_path {
    my ($path) = @_;
    $path =~ s{\\}{/}g if $^O eq 'MSWin32';
    return $path;
}

sub add_file {
    my ($name, $data) = @_;
    push @file_order, $name unless exists $files{$name};
    $files{$name} = $data;
}

my ($sdk, $sdk_real);
my @sdk_inputs;
if ($platform eq 'macos') {
    my ($index) = grep { $command[$_] eq '-syslibroot' } 0 .. $#command;
    die "A macOS SDK is required to prepare native linking\n" unless defined $index;
    $sdk = $command[$index + 1];
    $sdk_real = unix_path(abs_path($sdk));
    my $lib = File::Spec->catdir($sdk, 'usr', 'lib');
    push @directories, $lib;
    # Embed SDK link interfaces, not runtime dylibs or SDK headers.
    find({no_chdir => 1, wanted => sub {
        push @sdk_inputs, $File::Find::name if /\.tbd\z/ && -f $_;
    }}, $lib);
    for my $path (sort {
        join("\0", File::Spec->splitdir($a)) cmp join("\0", File::Spec->splitdir($b))
    } @sdk_inputs) {
        add_file('sdk/' . unix_path(File::Spec->abs2rel($path, $sdk)), read_file($path));
    }
}

sub resource {
    my ($path) = @_;
    $path = unix_path(abs_path($path));
    return "$root/$mapped{$path}" if exists $mapped{$path};
    my $name = defined($sdk_real) && index($path, "$sdk_real/") == 0
        ? 'sdk/' . substr($path, length($sdk_real) + 1)
        : 'files/' . scalar(@mapped_order) . '/' . basename($path);
    $mapped{$path} = $name;
    push @mapped_order, $path;
    my $data = read_file($path);
    if (index(substr($data, 0, 256), 'GNU ld script') >= 0) {
        $data =~ s{/\*.*?\*/}{}gs;
        # Relocate absolute paths and nested GROUP/AS_NEEDED link inputs.
        $data =~ s{(/[^\s()]+|lib[\w.+-]+\.(?:so(?:\.[\d]+)*|a))}{
            my $token = $1;
            '"' . resource(-f $token ? $token : resolve(":$token")) . '"';
        }ge;
        $scripts{$name} = 1;
    }
    add_file($name, $data);
    return "$root/$name";
}

my $static_only = $platform eq 'windows';
for (my $i = 0; $i < @command;) {
    my $arg = $command[$i++];
    if ($arg eq '-o' || $arg eq '-L') {
        ++$i;
    } elsif ($arg eq $obj || $arg =~ /^-L/) {
        next;
    } elsif ($arg eq '-dynamic-linker' || $arg eq '--dynamic-linker') {
        # The interpreter must remain an operating-system path.
        push @arguments, $arg, $command[$i++];
    } elsif ($arg eq '-syslibroot') {
        push @arguments, $arg, "$root/sdk";
        ++$i;
    } elsif ($arg eq '-Bstatic' || $arg eq '-Bdynamic') {
        $static_only = $arg eq '-Bstatic';
        push @arguments, $arg;
    } elsif ($arg =~ /^-l/) {
        push @arguments, resource(resolve(substr($arg, 2), $static_only));
    } elsif (-f $arg) {
        push @arguments, resource($arg);
    } else {
        push @arguments, $arg;
    }
}

my $blob = '';
my @manifest_files;
for my $name (@file_order) {
    my $data = $files{$name};
    push @manifest_files, {path => $name, offset => length($blob), size => length($data),
        script => $scripts{$name} ? JSON::PP::true : JSON::PP::false};
    $blob .= $data;
}
my $compressed = compress($blob, 9);
die "Cannot compress native link resources\n" unless defined $compressed;
# Small separate literals avoid MSVC's individual string literal limit.
my @literals;
for (my $i = 0; $i < length($compressed); $i += 1024) {
    push @literals, '"' . join('', map { sprintf '\\x%02x', $_ }
        unpack('C*', substr($compressed, $i, 1024))) . '"';
}
my $manifest = JSON::PP->new->ascii->canonical->encode(
    {target => $target, arguments => \@arguments, files => \@manifest_files});
my $content = "// Generated native link resources; do not edit.\n"
    . 'static constexpr size_t NativeRuntimeSize = ' . length($blob) . ";\n"
    . "static const char NativeRuntimeData[] =\n" . join("\n", @literals) . ";\n"
    . "static const char NativeRuntimeManifest[] = R\"CPSC(\n$manifest\n)CPSC\";\n";
write_file($output, $content) if !-e $output || read_file($output) ne $content;
# Track the actual toolchain inputs, not only the generator script.
write_file(File::Spec->catfile($directory, 'native-runtime-inputs.cmake'),
    "set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS\n"
    . join("\n", map { "  [==[$_]==]" } (@mapped_order, @sdk_inputs)) . ")\n");
print 'Embedded ' . scalar(@file_order) . ' native link inputs ('
    . length($compressed) . " compressed bytes)\n";
