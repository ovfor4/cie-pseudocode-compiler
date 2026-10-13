#!/bin/sh
set -eu

usage() {
    cat <<'EOF'
Usage: install.sh [--prefix DIR] [--source-dir DIR]

Install cpsc and its man page to DIR/bin and DIR/share/man/man1.
The default prefix is /usr/local. The source directory defaults to the
directory containing this script and must contain cpsc and doc/cpsc.1.
Use --source-dir build when running this script from the source checkout.
DESTDIR may be set to stage an installation without changing the prefix.
EOF
}

die() {
    printf 'install.sh: %s\n' "$*" >&2
    exit 1
}

prefix=/usr/local
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
while [ "$#" -gt 0 ]; do
    case "$1" in
        --prefix|--source-dir)
            [ "$#" -ge 2 ] || die "$1 requires a directory"
            [ -n "$2" ] || die "$1 requires a nonempty directory"
            case "$2" in --*) die "$1 requires a directory, got $2" ;; esac
            case "$1" in
                --prefix) prefix=$2 ;;
                --source-dir) source_dir=$2 ;;
            esac
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown argument: $1 (see --help)" ;;
    esac
done

case "$prefix" in /*) ;; *) die "--prefix must be an absolute path" ;; esac
destdir=${DESTDIR:-}
case "$destdir" in ''|/*) ;; *) die "DESTDIR must be an absolute path" ;; esac
source_dir=$(CDPATH= cd -- "$source_dir" && pwd) || die "cannot open source directory: $source_dir"
[ -f "$source_dir/cpsc" ] && [ -r "$source_dir/cpsc" ] && [ -x "$source_dir/cpsc" ] \
    || die "missing readable executable: $source_dir/cpsc; build cpsc first"
[ -f "$source_dir/doc/cpsc.1" ] && [ -r "$source_dir/doc/cpsc.1" ] \
    || die "missing readable manual: $source_dir/doc/cpsc.1; run make doc first"
command -v install >/dev/null 2>&1 || die "required command not found: install"

bin_dir=$destdir${prefix%/}/bin
man_dir=$destdir${prefix%/}/share/man/man1
install -d "$bin_dir" "$man_dir" \
    || die "cannot create installation directories; check permissions or choose a writable --prefix"
install -m 755 "$source_dir/cpsc" "$bin_dir/cpsc" \
    || die "cannot install cpsc to $bin_dir; check permissions"
install -m 644 "$source_dir/doc/cpsc.1" "$man_dir/cpsc.1" \
    || die "cannot install the manual to $man_dir; check permissions"

if [ "$(uname -s)" = Darwin ]; then
    command -v xattr >/dev/null 2>&1 || die "required command not found: xattr"
    attributes=$(xattr "$bin_dir/cpsc") \
        || die "cannot read extended attributes of $bin_dir/cpsc"
    if printf '%s\n' "$attributes" | grep -qx 'com\.apple\.quarantine'; then
        xattr -d com.apple.quarantine "$bin_dir/cpsc" \
            || die "cannot remove quarantine from $bin_dir/cpsc; check permissions"
    fi
fi

printf 'Installed %s\nInstalled %s\n' "$bin_dir/cpsc" "$man_dir/cpsc.1"
if [ -z "$destdir" ]; then
    case ":${PATH:-}:" in
        *":${prefix%/}/bin:"*) ;;
        *) printf 'Add %s/bin to PATH to run cpsc by name.\n' "${prefix%/}" ;;
    esac
    printf 'Read the manual with: man cpsc\n'
fi
