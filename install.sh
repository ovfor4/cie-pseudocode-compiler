#!/bin/sh
set -eu

usage() {
    cat <<'EOF'
Usage: install.sh [--prefix DIR] [--source-dir DIR]

Install cpsc to DIR/bin and its man pages to DIR/share/cpsc/man/man1 and man7.
The default prefix is /usr/local. The source directory defaults to the
directory containing this script and must contain cpsc, doc/cpsc.1 and
doc/language/*.7. Installation registers the independent manual search path.
Use --source-dir build when running this script from the source checkout.
DESTDIR may be set to stage files without changing host manpath configuration.
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
set -- "$source_dir"/doc/language/cpsc-*.7
[ -f "$1" ] || die "missing language manuals: $source_dir/doc/language; run make doc first"
for manual do
    [ -f "$manual" ] && [ -r "$manual" ] || die "missing readable manual: $manual"
done
command -v install >/dev/null 2>&1 || die "required command not found: install"

platform=$(uname -s)
man_root=${prefix%/}/share/cpsc/man
case "$man_root" in
    *:*) die "--prefix cannot contain a colon (manpath separator)" ;;
    *'
'*) die "--prefix cannot contain a newline" ;;
esac
if [ "$platform" = Linux ] && [ -z "$destdir" ]; then
    case "$man_root" in
        *[[:space:]]*) die "Linux man-db registration requires a --prefix without whitespace" ;;
    esac
fi

# Append only our entry; leave existing system/user configuration intact.
register_line() {
    config=$1
    entry=$2
    if [ -e "$config" ]; then
        [ -f "$config" ] && [ -r "$config" ] && [ -w "$config" ] \
            || die "cannot update manpath configuration: $config"
        if grep -Fqx -- "$entry" "$config"; then return; fi
    fi
    printf '\n# cpsc manual: %s\n%s\n' "$man_root" "$entry" >> "$config" \
        || die "cannot register manuals in $config"
    printf 'Registered manuals in %s\n' "$config"
}

register_manpath() {
    case "$platform" in
        Linux)
            if [ "$(id -u)" -eq 0 ]; then
                if [ -f /etc/manpath.config ]; then
                    config=/etc/manpath.config
                elif [ -f /etc/man_db.conf ]; then
                    config=/etc/man_db.conf
                else
                    die "cannot locate man-db configuration; register $man_root manually"
                fi
            else
                [ -n "${HOME:-}" ] || die "HOME is unset; cannot locate user manpath configuration"
                config=$HOME/.manpath
            fi
            register_line "$config" "MANDATORY_MANPATH $man_root"
            ;;
        Darwin)
            if [ "$(id -u)" -eq 0 ]; then
                install -d /usr/local/etc/man.d || die "cannot create macOS man configuration directory"
                register_line /usr/local/etc/man.d/cpsc.conf "MANPATH $man_root"
            else
                [ -n "${HOME:-}" ] || die "HOME is unset; cannot locate user shell configuration"
                case "${SHELL:-}" in
                    */zsh) config=$HOME/.zprofile ;;
                    */bash) config=$HOME/.bash_profile ;;
                    *)
                        printf 'Set MANPATH manually for your shell: %s:\n' "$man_root"
                        config=
                        ;;
                esac
                # Quote a literal path inside double quotes in the shell snippet.
                shell_man_root=$(printf '%s' "$man_root" | sed 's/[\\`"$]/\\&/g')
                if [ -n "$config" ]; then
                    marker="# cpsc manual: $man_root"
                    if ! grep -Fqx -- "$marker" "$config" 2>/dev/null; then
                        printf '\n%s\n%s\n%s\n%s\n%s\n' "$marker" \
                            'case ":${MANPATH:-}:" in' \
                            "    *\":$shell_man_root:\"*) ;;" \
                            "    *) export MANPATH=\"$shell_man_root:\${MANPATH:-}\" ;;" \
                            'esac' >> "$config" || die "cannot update shell configuration: $config"
                    fi
                    printf 'Registered manuals in %s; open a new login terminal.\n' "$config"
                fi
                printf '%s\n' 'For optional global registration, run manually:' \
                    '  sudo mkdir -p /usr/local/etc/man.d' \
                    "  printf '%s\\n' \"MANPATH $shell_man_root\" | sudo tee -a /usr/local/etc/man.d/cpsc.conf"
            fi
            ;;
        *) printf 'Register %s in your system or user manpath configuration.\n' "$man_root" ;;
    esac
}

bin_dir=$destdir${prefix%/}/bin
man_dir=$destdir$man_root
install -d "$bin_dir" "$man_dir/man1" "$man_dir/man7" \
    || die "cannot create installation directories; check permissions or choose a writable --prefix"
install -m 755 "$source_dir/cpsc" "$bin_dir/cpsc" \
    || die "cannot install cpsc to $bin_dir; check permissions"
install -m 644 "$source_dir/doc/cpsc.1" "$man_dir/man1/cpsc.1" \
    || die "cannot install the manual to $man_dir; check permissions"

for manual do
    install -m 644 "$manual" "$man_dir/man7/$(basename -- "$manual")" \
        || die "cannot install language manual: $manual"
done

if [ "$platform" = Darwin ]; then
    command -v xattr >/dev/null 2>&1 || die "required command not found: xattr"
    attributes=$(xattr "$bin_dir/cpsc") \
        || die "cannot read extended attributes of $bin_dir/cpsc"
    if printf '%s\n' "$attributes" | grep -qx 'com\.apple\.quarantine'; then
        xattr -d com.apple.quarantine "$bin_dir/cpsc" \
            || die "cannot remove quarantine from $bin_dir/cpsc; check permissions"
    fi
fi

printf 'Installed %s\nInstalled manuals in %s\n' "$bin_dir/cpsc" "$man_dir"
if [ -z "$destdir" ]; then
    register_manpath
    case ":${PATH:-}:" in
        *":${prefix%/}/bin:"*) ;;
        *) printf 'Add %s/bin to PATH to run cpsc by name.\n' "${prefix%/}" ;;
    esac
    printf 'Read manuals with: man 1 cpsc; man 7 cpsc-language-overview\n'
    if [ -n "${MANPATH:-}" ]; then
        printf 'Your explicit MANPATH overrides configuration; also include %s (retain a colon for defaults).\n' "$man_root"
    fi
else
    printf 'Staged files only; register %s in manpath when deploying.\n' "$man_root"
fi
