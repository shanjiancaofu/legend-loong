"""clang-format entry for pre-commit; --staged retains the legacy read-only check.
Set CLANG_FORMAT to override executable discovery. Requires clang-format 22+.
--fix formats the files supplied by pre-commit; neither mode stages files.
"""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

def git(*args):
    return subprocess.check_output(["git", *args])

def executable():
    override = os.environ.get("CLANG_FORMAT")
    if override:
        return override
    found = shutil.which("clang-format")
    if found:
        return found
    candidates = [
        Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe",
        Path("D:/tool/Espressif/tools/esp-clang/esp-19.1.2_20250312/esp-clang/bin/clang-format.exe"),
    ]
    for path in candidates:
        if path.is_file():
            return str(path)
    raise RuntimeError("clang-format not found. Install it or set CLANG_FORMAT.")

def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--staged", action="store_true")
    mode.add_argument("--fix", action="store_true")
    parser.add_argument("filenames", nargs="*")
    args = parser.parse_args()
    root = Path(git("rev-parse", "--show-toplevel").decode().strip())
    os.chdir(root)
    if args.fix:
        files = [name for name in args.filenames
                 if name.startswith("Source/") and Path(name).suffix in (".h", ".cpp")]
        if not files:
            return 0
        tool = executable()
        version = subprocess.check_output([tool, "--version"]).decode("utf-8", errors="replace")
        major = re.search(r"version (\d+)", version)
        if not major or int(major.group(1)) < 22:
            raise RuntimeError("clang-format 22+ is required by this project's style config.")
        subprocess.run([tool, "--style=file:" + str(root / "Source/.clang-format"), "-i", *files], check=True)
        return 0
    names = git("diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z").decode("utf-8").split("\0")
    files = [name for name in names if name.startswith("Source/") and Path(name).suffix in (".h", ".cpp")]
    if not files:
        print("Loong pre-commit: no staged C++ files.")
        return 0
    try:
        config = git("show", ":Source/.clang-format")
    except subprocess.CalledProcessError:
        raise RuntimeError("Stage Source/.clang-format before committing C++ files.")
    tool = executable()
    version = subprocess.check_output([tool, "--version"]).decode("utf-8", errors="replace")
    major = re.search(r"version (\d+)", version)
    if not major or int(major.group(1)) < 22:
        raise RuntimeError("clang-format 22+ is required by this project's style config.")
    mismatches = []
    with tempfile.TemporaryDirectory(prefix="loong-format-") as directory:
        style = Path(directory) / ".clang-format"
        style.write_bytes(config)
        for name in files:
            original = git("show", ":" + name)
            formatted = subprocess.run(
                [tool, "--style=file:" + str(style), "--assume-filename=" + str(root / name)],
                input=original, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
            ).stdout
            if original.replace(b"\r\n", b"\n") != formatted.replace(b"\r\n", b"\n"):
                mismatches.append(name)
    if mismatches:
        print("Loong pre-commit: format these files, review, then stage them again:", file=sys.stderr)
        for name in mismatches:
            print("  " + name, file=sys.stderr)
        print("No files or index entries were modified.", file=sys.stderr)
        return 1
    print(f"Loong pre-commit: {len(files)} staged C++ files passed.")
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        print(f"Loong pre-commit: {error}", file=sys.stderr)
        sys.exit(1)
