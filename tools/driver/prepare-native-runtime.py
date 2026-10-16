#!/usr/bin/env python3
"""Freeze the build toolchain's native link inputs into the compiler."""

import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--platform", choices=("linux", "windows", "macos"), required=True)
    parser.add_argument("--flag", action="append", default=[])
    args = parser.parse_args()
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    def compiler(*options):
        return subprocess.run([args.compiler, *args.flag, *options], check=True,
                              capture_output=True, text=True)

    # An actual object lets the driver validate its inputs without linking a probe.
    source = output.parent / "native-probe.cc"
    obj = output.parent / "native-probe.o"
    binary = output.parent / "native-probe.out"
    source.write_text("int main() { return 0; }\n")
    compiler("-c", str(source), "-o", str(obj))
    static = [] if args.platform == "macos" else (
        ["-static"] if args.platform == "windows" else
        ["-static-libstdc++", "-static-libgcc"])
    trace = compiler("-###", "-fuse-ld=lld", *static, str(obj), "-o", str(binary))
    commands = [shlex.split(line.strip()) for line in trace.stderr.splitlines()
                if line.lstrip().startswith('"')]
    if len(commands) != 1:
        raise RuntimeError("Expected one native linker command: " + trace.stderr)
    command = commands[0][1:]
    directories = [Path(arg[2:]) for arg in command if arg.startswith("-L")]
    target = compiler("-dumpmachine").stdout.strip()
    files = {}
    scripts = set()
    arguments = []
    root = "@CPSC_RUNTIME@"

    def resolve(name, static_only=False):
        if name.startswith(":"):
            names = [name[1:]]
        elif args.platform == "macos":
            names = ["lib" + name + suffix for suffix in (".tbd", ".dylib", ".a")]
        elif static_only:
            names = ["lib" + name + ".a"]
        else:
            names = ["lib" + name + suffix for suffix in (".dll.a", ".so", ".a")]
        for directory in directories:
            for filename in names:
                path = directory / filename
                if path.is_file():
                    return path
        for filename in names:
            found = Path(compiler("-print-file-name=" + filename).stdout.strip())
            if found.is_file():
                return found
        raise RuntimeError("Cannot find native link input " + name)

    sdk = None
    if args.platform == "macos":
        if "-syslibroot" not in command:
            raise RuntimeError("A macOS SDK is required to prepare native linking")
        sdk = Path(command[command.index("-syslibroot") + 1])
        directories += [sdk / "usr/lib"]
        # System dylibs live in the dyld cache on users' machines. Embed their
        # SDK link interfaces, not runtime dylibs or SDK headers.
        for path in sorted((sdk / "usr/lib").rglob("*.tbd")):
            files["sdk/" + path.relative_to(sdk).as_posix()] = path.read_bytes()

    mapped = {}

    def resource(path):
        path = path.resolve()
        if path in mapped:
            return root + "/" + mapped[path]
        if sdk and path.is_relative_to(sdk.resolve()):
            name = "sdk/" + path.relative_to(sdk.resolve()).as_posix()
        else:
            name = "files/" + str(len(mapped)) + "/" + path.name
        mapped[path] = name
        data = path.read_bytes()
        if b"GNU ld script" in data[:256]:
            text = re.sub(r"/\*.*?\*/", "", data.decode(), flags=re.S)
            # GNU libc/libm linker scripts contain absolute paths and nested
            # GROUP/AS_NEEDED inputs. Relocate every referenced link input.
            pattern = r"(?:/[^\s()]+|lib[\w.+-]+\.(?:so(?:\.[\d]+)*|a))"

            def relocate(match):
                token = match.group()
                dependency = Path(token)
                if not dependency.is_file():
                    dependency = resolve(":" + token)
                return resource(dependency)

            data = re.sub(pattern, relocate, text).encode()
            scripts.add(name)
        files[name] = data
        return root + "/" + name

    static_only = args.platform == "windows"
    i = 0
    while i < len(command):
        arg = command[i]
        i += 1
        if arg == "-o":
            i += 1
        elif arg == str(obj):
            pass
        elif arg == "-L":
            i += 1
        elif arg.startswith("-L"):
            pass
        elif arg in ("-dynamic-linker", "--dynamic-linker"):
            # The interpreter must remain an operating-system path.
            arguments += [arg, command[i]]
            i += 1
        elif arg == "-syslibroot":
            arguments += [arg, root + "/sdk"]
            i += 1
        elif arg in ("-Bstatic", "-Bdynamic"):
            static_only = arg == "-Bstatic"
            arguments.append(arg)
        elif arg.startswith("-l"):
            arguments.append(resource(resolve(arg[2:], static_only)))
        elif Path(arg).is_file():
            arguments.append(resource(Path(arg)))
        else:
            arguments.append(arg)

    blob = bytearray()
    manifest = {"target": target, "arguments": arguments, "files": []}
    for name, data in files.items():
        manifest["files"].append({"path": name, "offset": len(blob),
                                  "size": len(data), "script": name in scripts})
        blob.extend(data)
    compressed = zlib.compress(blob, 9)
    # Small separate literals avoid MSVC's individual string literal limit.
    literals = "\n".join('"' + "".join(f"\\x{b:02x}" for b in compressed[i:i+1024]) + '"'
                         for i in range(0, len(compressed), 1024))
    content = ("// Generated native link resources; do not edit.\n"
               f"static constexpr size_t NativeRuntimeSize = {len(blob)};\n"
               "static const char NativeRuntimeData[] =\n" + literals + ";\n"
               'static const char NativeRuntimeManifest[] = R"CPSC(\n' +
               json.dumps(manifest) + '\n)CPSC";\n')
    if not output.exists() or output.read_text() != content:
        output.write_text(content)
    # Track the actual toolchain inputs, not only the generator script.
    dependencies = [str(path) for path in mapped]
    if sdk:
        dependencies += [str(path) for path in (sdk / "usr/lib").rglob("*.tbd")]
    (output.parent / "native-runtime-inputs.cmake").write_text(
        "set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS\n" +
        "\n".join('  [==[' + path + ']==]' for path in dependencies) + ")\n")
    print(f"Embedded {len(files)} native link inputs ({len(compressed)} compressed bytes)")


if __name__ == "__main__":
    main()
