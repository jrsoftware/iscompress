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

## Test

```
test\test.bat x86|x64
```

Round-trips every codec, then reports MB/s. Run `build.bat` first.

ARM64EC (iszstd only) can't be tested this way: those binaries run only on Windows 11 ARM64. Verify them statically with `dumpbin` instead.

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
- zstd: `ZSTD_MULTITHREAD` (iszstd only), `ZSTD_NO_TRACE`

`Z_SOLO` tells zlib to exclude all OS and C-library dependencies (`<stdio.h>`, `<stdlib.h>`, `<stddef.h>`, file/gzip APIs, `zcalloc`/`zcfree`). This is what makes the CRT-free DLL build possible, but it means the Inno Setup-specific `.c` files must supply any memory functions (`zmemcpy`, `zmemzero`) and other symbols that zlib would otherwise get from the standard library. When updating zlib, check for new references to such symbols.

### Project file consistency

The `.vcxproj` files are maintained in three groups whose members share the same structure:

- **Compression, CRT-free** — `bzlib/isbzip.vcxproj` and `zlib/iszlib.vcxproj`
- **Decompression, CRT-free** — `bzlib/isbunzip.vcxproj`, `zlib/isunzlib.vcxproj`, and `zstd/isunzstd.vcxproj`
- **Compression, full CRT** — `zstd/iszstd.vcxproj` and issrc's `islzma.vcxproj` (`Projects\Src\Compression.LZMACompressor\islzma\islzma.vcxproj`)

Within a group the projects intentionally differ **only** in these per-library specifics:

- `isunzstd`'s `<IntrinsicFunctions>true</IntrinsicFunctions>` (`/Oi`) in its Debug configs
- preprocessor defines
- `bzip2`'s extra `chkstk.obj` link dependency
- the `.def` file
- `BaseAddress`
- target platform (the `zstd` projects use `ARM64EC`, `bzip2`/`zlib` use `ARM64`)
- the list of source files

### Build constraint

Why the helper files call `RtlFillMemory`/`RtlMoveMemory` instead of looping, and why they are compiled without `/GL` anyway: see `bzlib/innosetup.c`.

Win32/x64 set `<LinkTimeCodeGeneration>UseLinkTimeCodeGeneration</LinkTimeCodeGeneration>` because full `/LTCG` is wanted and `Microsoft.Cpp.WholeProgramOptimization.props` gives those two `/LTCG:incremental` otherwise. ARM64/ARM64EC already default to full `/LTCG`.
