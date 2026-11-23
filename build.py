"""Build KILN with its bundled, patched Open-XY SDK."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent
VENDOR = ROOT / "vendor"
MACHINE = ROOT / "kiln"
TOOLS = ROOT / ".kiln" / "toolchain"


def unpack_sdk(folder):
    manifest = json.loads((VENDOR / "manifest.json").read_bytes())
    archive = VENDOR / manifest["sdk"]
    for path, expected in [(archive, manifest["sha256"]), *(
        (ROOT / name, digest) for name, digest in manifest["patches"].items()
    )]:
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Dependency checksum mismatch for {path.name}")
    with zipfile.ZipFile(archive) as sdk:
        sdk.extractall(folder)
        for entry in sdk.infolist():
            if entry.external_attr >> 16 & 0o111:
                (folder / entry.filename).chmod(0o755)


def install_tools(run, target, archive):
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        run("toolchain", "fetch", "--dir", target)
        return
    with tempfile.TemporaryDirectory(dir=target.parent) as temp:
        staged = Path(temp) / "toolchain"
        args = ["--archive", archive] if archive else []
        run("toolchain", "fetch", "--dir", staged, *args)
        try:
            staged.rename(target)
        except OSError:
            if not target.exists():
                raise
            run("toolchain", "fetch", "--dir", target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest="action", required=True)
    subs.add_parser("test", help="run the machine checks and host C++ tests")
    setup = subs.add_parser("tools", help="install the SDK-pinned compiler for this host")
    setup.add_argument("--archive", type=Path, help="use a local reviewed compiler ZIP")
    for name in ("compile", "build"):
        cmd = subs.add_parser(name)
        cmd.add_argument("--out", type=Path, required=True, help="new output directory")
        cmd.add_argument("--toolchain", type=Path, default=TOOLS, help="existing compiler root containing bin/")
        cmd.add_argument("--layout", type=Path, help="reuse machine numbers from an earlier build")
        if name == "build":
            cmd.add_argument("--stock-os", type=Path, required=True, help="your original OS 1.2.1 os.ldr")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="kiln-sdk-") as temp:
        sdk = Path(temp)
        unpack_sdk(sdk)

        def run(*parts):
            subprocess.run([sys.executable, str(sdk / "sdk.py"), *map(str, parts)], check=True)

        if args.action == "test":
            run("test", "--machine", MACHINE, "--no-cpu")
        elif args.action == "tools":
            install_tools(run, TOOLS, args.archive)
        else:
            tc = args.toolchain.resolve()
            if not tc.is_dir():
                parser.error(f"Missing toolchain at {tc}. Run the tools command or supply --toolchain.")
            layout = ["--layout", args.layout.resolve()] if args.layout else []
            stock = ["--stock-os", args.stock_os.resolve()] if args.action == "build" else []
            run(args.action, "--machine", MACHINE, "--compiler", tc, "--toolchain", tc / "bin",
                "--profiling", "off", "--out", args.out.resolve(), *layout, *stock)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        sys.exit(exc.returncode)
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        sys.exit(str(exc))
