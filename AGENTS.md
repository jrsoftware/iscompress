# iscompress AI Agent Guide

## Overview

This repo produces four lightweight compression/decompression DLLs for Inno Setup
- **isbzip.dll** — bzip2 compression (exports `BZ2_bzCompressInit`, `BZ2_bzCompress`, `BZ2_bzCompressEnd`)
- **isbunzip.dll** — bzip2 decompression (exports `BZ2_bzDecompressInit`, `BZ2_bzDecompress`, `BZ2_bzDecompressEnd`)
- **iszlib.dll** — zlib compression (exports `deflateInit_`, `deflate`, `deflateEnd`)
- **isunzlib.dll** — zlib decompression (exports `inflateInit_`, `inflate`, `inflateEnd`, `inflateReset`)

Each DLL has x86, x64 (suffixed `-x64`), and ARM64 (suffixed `-Arm64`) variants.

## Vendor source

- `bzlib/` — bzip2. Upstream: https://sourceware.org/bzip2/
- `zlib/` — zlib. Upstream: https://zlib.net/

Only the source files actually needed by the DLLs are included — not the full upstream archives. The vendor `.c` and `.h` files are unmodified upstream source converted to Windows EOL. Do not change them unless updating to a new upstream version.

Three files are **not** vendor source — they are Inno Setup-specific and live alongside the vendor files:
- `bzlib/innosetup.c`
- `zlib/iszlib.c`
- `zlib/isunzlib.c`

### Updating to a new version

Follow the pattern in git history: replace all files with the new version archive contents and commit with a message like `Update to XX.YY from <URL>`.

When reviewing update commits, focus on detecting a compromised upstream release: obfuscated or suspiciously complex new logic (especially in compression/decompression routines or build scripts), hidden functionality in test/utility code, unexpected binary blobs, and changes that don't match the stated release notes or changelog.

## Build

Requires Visual Studio 2022 with C++ tools (v143 toolset). Uses `msbuild.exe` via `vcvarsall.bat`.

Two settings files (not checked in) are needed:
- `compilesettings.bat` — sets `VSBUILDROOT` to VS build tools path (e.g. `c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build`)
- `buildsettings.bat` — sets `ISSRCROOT` to the Inno Setup source root (e.g. `c:\Coding\Is\Issrc`)

**Compile a single architecture:**
```
compile.bat x86|x64|arm64
```

**Build x86+x64 and copy DLLs to issrc:**
```
build.bat [noclean] [nosynch]
```

The `nosynch` flag skips synching to the issrc `Projects\Bin` folder.

## Architecture

Each vendor library directory (`bzlib/`, `zlib/`) contains one `.sln` with two projects — a compression DLL and a decompression DLL. The `.def` files control which functions each DLL exports.

### CRT-free DLLs

All four DLLs are built without the C runtime — Release builds set `IgnoreAllDefaultLibraries` and use `_DllMainCRTStartup` as the entry point. The three Inno Setup-specific `.c` files provide the minimal runtime support each DLL needs:

- `bzlib/innosetup.c` — used by both isbzip and isbunzip. Provides `bz_internal_error` (raises a Windows exception), null `malloc`/`free` stubs, `memset`, and the entry point
- `zlib/iszlib.c` — used by iszlib. Provides `memset` and the entry point
- `zlib/isunzlib.c` — used by isunzlib. Provides `memset`, `zmemcpy`, `zmemzero`, and the entry point

### Key preprocessor defines

- bzip2: `BZ_NO_STDIO`, `BZ_FOR_INNO_SETUP`
- zlib: `NO_GZIP`, `Z_SOLO`

### Build constraint

Whole-program optimization (`/GL`) is enabled at the solution level but **disabled** at the `ClCompile` level (`<WholeProgramOptimization>false</WholeProgramOptimization>`). This is required because VS2022's optimizer replaces assignment loops with calls to `memset`, which breaks when `memset` is custom-implemented. Do not re-enable `/GL` at the compile level.
