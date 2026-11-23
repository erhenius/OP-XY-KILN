# Building KILN 0.2.5

## Using TE Builder

1. Download the repository ZIP or the standalone source release.
2. In TE Builder’s XY section, choose **Add machine / Import source** and select the whole ZIP. The bundle manifest sits beside the `kiln` folder.
3. Choose **KILN 0.2.5** for the build queue, load your normal machine selection and use your **4 MiB** profile. The bundle specifies Open-XY commit `ca122f4062f08f13a3023904620a4c35ff9b4a45`.
4. Build and review the final memory total. Stage the verified candidate to the XY you built it for. After installation select KILN on FX I or FX II.

TE Builder supplies the expanded memory support itself. It reads the KILN meter patch from the bundle manifest. The archive in `vendor/` supplies the separate command-line workflow.

## Using the standalone source

Extract the ZIP. Open a terminal in the folder containing `build.py`. The examples use `python3.12`; on Windows use `py -3.12`. Python 3.11 or later is required. The build wrapper uses the standard library and unpacks its checksum-verified SDK into a temporary directory for each command.

### 1. Get the compiler

On Apple Silicon macOS or Windows x86-64, run once per extracted repository:

```sh
python3.12 build.py tools
```

The SDK downloads the Blackfin compiler and binutils for this host, checks their pinned hashes and installs them in `.kiln/toolchain/`. For an offline install using a previously downloaded official toolchain archive:

```sh
python3.12 build.py tools --archive "/path/to/open-xy-toolchain-darwin-arm64.zip"
```

If TE Builder already has the authenticated compiler, skip this step and pass `--toolchain "/path/to/toolchain-root"` on the compile or build command. Choose the directory **containing** `bin`. The SDK verifies the tools during the build.

On Linux x86-64, this SDK pin provides a source-build recipe for the compiler. Follow “On Linux” in the upstream build guide and pass the resulting toolchain root with `--toolchain`. The same guide and Linux build scripts are included in the vendored SDK ZIP. [Pinned upstream build guide](https://github.com/etcetc000/open-xy/blob/ca122f4062f08f13a3023904620a4c35ff9b4a45/docs/build.md)

### 2. Check or compile the module

With a host C++11 compiler available as `c++`, the five source tests run locally:

```sh
python3.12 build.py test
```

With the Blackfin tools installed, compile and measure the native module:

```sh
python3.12 build.py compile --out ../kiln-module
```

Use a new output directory for each run. The compile command can run without a connected XY or a stock OS file.

### 3. Build the firmware candidate

Supply the original OP-XY **OS 1.2.1** `os.ldr` from your own device or local verified stock backup. The SDK expects SHA-256 `60358afdaa9b58ed40411602f6d5d4e1e3e02553ff2da7e25038e71dd002b52f`. Keep the OS and generated firmware outside the repository.

```sh
python3.12 build.py build --stock-os "/path/to/stock-1.2.1/os.ldr" --out ../kiln-firmware
```

For an existing KILN installation, append `--layout "/path/to/previous/build/layout.json"` to reuse machine numbers from a **4 MiB** build. Save the resulting `layout.json` for later builds. A layout carries machine numbers. A full collection also needs its machine sources, which TE Builder manages for you.

On success the SDK writes the device files under `candidate/`, the memory report under `analysis/` and the verification result in `verification.json`. The build command ends on disk. Use TE Builder’s normal workflow for managed installations, or follow the pinned SDK guide for a standalone candidate.

## Dependency inventory

| Component | How it is supplied |
| --- | --- |
| KILN DSP, tables, tests, color and grayscale screens | Source in `kiln/` |
| Open-XY SDK, headers, linker, profiles and supporting files | Complete source snapshot in `vendor/open-xy-1.17-kiln.zip` |
| Stereo return meters | Applied in the snapshot and separately available in `integration/return-meter.patch` for TE Builder |
| Expanded 4 MiB layout, link and size checks | Applied in the snapshot and available in `vendor/expanded-native.patch` |
| Python 3.11+ and host C++ compiler for tests | Supplied by the build host |
| Pinned platform compiler and binutils | Installed with `build.py tools`, supplied as an offline archive, or reused with `--toolchain` |
| Stock firmware | Supplied locally by the device owner |

The snapshot is made from the upstream commit in `vendor/manifest.json` with the two listed patches applied. Both the archive and patch digests are checked by `build.py`. The SDK license and component notices are inside the snapshot. The machine license is at the repository root.
