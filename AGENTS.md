# iscompress AI Agent Guide

## Overview

This repo produces six lightweight compression/decompression DLLs for Inno Setup
- **isbzip.dll** — bzip2 compression (exports `BZ2_bzCompressInit`, `BZ2_bzCompress`, `BZ2_bzCompressEnd`)
- **isbunzip.dll** — bzip2 decompression (exports `BZ2_bzDecompressInit`, `BZ2_bzDecompress`, `BZ2_bzDecompressEnd`)
- **iszlib.dll** — zlib compression (exports `deflateInit_`, `deflate`, `deflateEnd`)
- **isunzlib.dll** — zlib decompression (exports `inflateInit_`, `inflate`, `inflateEnd`, `inflateReset`)
- **iszstd.dll** — zstd compression (exports `ZSTD_createCStream`, `ZSTD_initCStream`, `ZSTD_compressStream2`, `ZSTD_freeCStream`, `ZSTD_CCtx_setParameter`, `ZSTD_CCtx_reset`, `ZSTD_getFrameProgression`, `ZSTD_isError`)
- **isunzstd.dll** — zstd decompression (exports `ZSTD_createDStream`, `ZSTD_initDStream`, `ZSTD_decompressStream`, `ZSTD_freeDStream`, `ZSTD_isError`)

Each DLL has x86 and x64 (suffixed `-x64`) variants. `iszstd.dll` additionally has a Arm64EC variant (suffixed `-Arm64EC`). The number of built DLL files is therefore thirteen and not six.

## Vendor source

- `bzlib/` — bzip2. Upstream: https://sourceware.org/bzip2/
- `zlib/` — zlib. Upstream: https://zlib.net/
- `zstd/` — zstd. Upstream: https://github.com/facebook/zstd

Only the source files actually needed by the DLLs are included — not the full upstream archives. The vendor `.c` and `.h` files are unmodified upstream source converted to Windows EOL. Do not change them unless updating to a new upstream version.

These files are **not** vendor source — they are Inno Setup-specific helper files and live alongside the vendor files:
- `bzlib/innosetup.c`
- `zlib/iszlib.c`
- `zlib/isunzlib.c`
- `zstd/isunzstd.c`

### Updating to a new version

Follow the pattern in git history: replace all files with the new version archive contents and commit with a message like `Update to XX.YY from <URL>`.

When reviewing update commits, focus on detecting a compromised upstream release: obfuscated or suspiciously complex new logic (especially in compression/decompression routines or build scripts), hidden functionality in test/utility code, unexpected binary blobs, and changes that don't match the stated release notes or changelog.

## Build

Requires Visual Studio 2022 with C++ tools (v143 toolset). Uses `msbuild.exe` via `vcvarsall.bat`.

Two settings files (not checked in) are needed:
- `compilesettings.bat` — sets `VSBUILDROOT` to VS build tools path
- `buildsettings.bat` — sets `ISSRCROOT` to the Inno Setup source root

**Compile a single architecture:**
```
compile.bat x86|x64|arm64ec
```

**Build all DLLs and copy them to issrc:**
```
build.bat [noclean] [nosynch]
```

The `nosynch` flag skips synching to the issrc `Projects\Bin` folder.

## Architecture

Each vendor library directory contains one `.sln` with two projects — a compression DLL and a decompression DLL. The `.def` files control which functions each DLL exports.

### CRT-free DLLs

All DLLs except `iszstd.dll` are built without the C runtime — Release builds set `IgnoreAllDefaultLibraries` and use `_DllMainCRTStartup` as the entry point. The aforementioned helper files provide the minimal runtime support each DLL needs:

- `bzlib/innosetup.c` — used by both isbzip and isbunzip. Provides `bz_internal_error` (raises a Windows exception), null `malloc`/`free` stubs, `memset`, and the entry point
- `zlib/iszlib.c` — used by iszlib. Provides `memset` and the entry point
- `zlib/isunzlib.c` — used by isunzlib. Provides `memset`, `zmemcpy`, `zmemzero`, and the entry point
- `zstd/isunzstd.c` — used by isunzstd. Provides heap-backed `malloc`/`free`/`calloc`, `memset`/`memcpy`/`memmove`, the entry point, and — on x86 — the compiler-helper routines `__allmul` and `__allshl`

### Key preprocessor defines

- bzip2: `BZ_NO_STDIO`
- zlib: `NO_GZIP`, `Z_SOLO`
- zstd: `ZSTD_MULTITHREAD` (iszstd only)

`Z_SOLO` tells zlib to exclude all OS and C-library dependencies (`<stdio.h>`, `<stdlib.h>`, `<stddef.h>`, file/gzip APIs, `zcalloc`/`zcfree`). This is what makes the CRT-free DLL build possible, but it means the Inno Setup-specific `.c` files must supply any memory functions (`zmemcpy`, `zmemzero`) and other symbols that zlib would otherwise get from the standard library. When updating zlib, check for new references to such symbols.

### Build constraint

For the CRT-free DLLs, whole-program optimization (`/GL`) is enabled at the solution level but **disabled** at the `ClCompile` level (`<WholeProgramOptimization>false</WholeProgramOptimization>`). This is required because VS2022's optimizer replaces assignment loops with calls to `memset`, which breaks when `memset` is custom-implemented. Do not re-enable `/GL` at the compile level for those DLLs.
