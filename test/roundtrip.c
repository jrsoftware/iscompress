/*
 * Round-trip test for the Inno Setup compression DLLs.
 *
 * For each codec it loads the compression and decompression DLLs with
 * LoadLibrary/GetProcAddress, compresses a mixed test buffer, decompresses the
 * result, and checks it matches the original bytes.
 *
 * The DLLs are compiled /Gz, so every exported function - and every callback
 * the DLLs call back into (the zlib/bzip2 allocators) - is __stdcall.
 *
 * Build and run via test.bat, which copies the matching DLLs next to the EXE
 * under their base names (iszstd.dll, isunzstd.dll, iszlib.dll, ...).
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N       200000u          /* test buffer size */
#define COMPCAP (N + 65536u)     /* output capacity, above every codec's bound */

/* Build a buffer that mixes compressible repetition with incompressible LCG
   noise, so both match-copying and entropy coding get exercised. */
static void make_buffer(unsigned char *b, size_t n)
{
    size_t i;
    unsigned int lcg = 12345u;
    for (i = 0; i < n; i++) {
        lcg = lcg * 1103515245u + 12345u;
        b[i] = (i % 61 < 40) ? (unsigned char)('A' + (i % 26))
                             : (unsigned char)(lcg >> 24);
    }
}

/* zlib (Z_SOLO) and bzip2 (its built-in malloc is a NULL stub) both leave
   memory allocation to the caller. The DLLs are /Gz, so these are __stdcall. */
static void * __stdcall z_alloc(void *opaque, unsigned items, unsigned size)
{ (void)opaque; return malloc((size_t)items * size); }
static void __stdcall z_free(void *opaque, void *addr)
{ (void)opaque; free(addr); }

static void * __stdcall bz_alloc(void *opaque, int n, int m)
{ (void)opaque; return malloc((size_t)n * m); }
static void __stdcall bz_free(void *opaque, void *p)
{ (void)opaque; free(p); }

#define GET(h, type, fn) \
    type fn = (type)GetProcAddress(h, #fn); \
    if (!fn) { printf("  missing export %s\n", #fn); return 1; }

static void report(const char *name, unsigned comp, int ok)
{
    printf("  %-6s orig=%u compress=%u decompress=%u %s\n",
           name, (unsigned)N, comp, (unsigned)N, ok ? "OK" : "FAIL");
}

/* ---------------------------------------------------------------- zstd ---- */

typedef struct { const void *src; size_t size; size_t pos; } ZSTD_inBuffer;
typedef struct { void *dst; size_t size; size_t pos; } ZSTD_outBuffer;

typedef void *   (__stdcall *fn_createCStream)(void);
typedef size_t   (__stdcall *fn_initCStream)(void *, int);
typedef size_t   (__stdcall *fn_compressStream2)(void *, ZSTD_outBuffer *, ZSTD_inBuffer *, int);
typedef size_t   (__stdcall *fn_freeCStream)(void *);
typedef unsigned (__stdcall *fn_isError)(size_t);
typedef void *   (__stdcall *fn_createDStream)(void);
typedef size_t   (__stdcall *fn_initDStream)(void *);
typedef size_t   (__stdcall *fn_decompressStream)(void *, ZSTD_outBuffer *, ZSTD_inBuffer *);
typedef size_t   (__stdcall *fn_freeDStream)(void *);

static int test_zstd(const unsigned char *orig, unsigned char *comp, unsigned char *deco)
{
    HMODULE hc = LoadLibraryA("iszstd.dll");
    HMODULE hd = LoadLibraryA("isunzstd.dll");
    unsigned comp_size;
    void *cs, *ds;
    size_t rem;
    int ok;

    if (!hc || !hd) { printf("  LoadLibrary failed (iszstd/isunzstd)\n"); return 1; }
    GET(hc, fn_createCStream,    ZSTD_createCStream)
    GET(hc, fn_initCStream,      ZSTD_initCStream)
    GET(hc, fn_compressStream2,  ZSTD_compressStream2)
    GET(hc, fn_freeCStream,      ZSTD_freeCStream)
    GET(hc, fn_isError,          ZSTD_isError)
    GET(hd, fn_createDStream,    ZSTD_createDStream)
    GET(hd, fn_initDStream,      ZSTD_initDStream)
    GET(hd, fn_decompressStream, ZSTD_decompressStream)
    GET(hd, fn_freeDStream,      ZSTD_freeDStream)

    cs = ZSTD_createCStream();
    if (!cs) { printf("  ZSTD_createCStream null\n"); return 1; }
    ZSTD_initCStream(cs, 6);
    {
        ZSTD_inBuffer  in  = { orig, N, 0 };
        ZSTD_outBuffer out = { comp, COMPCAP, 0 };
        do {
            rem = ZSTD_compressStream2(cs, &out, &in, /*ZSTD_e_end*/2);
            if (ZSTD_isError(rem)) { printf("  zstd compress error\n"); ZSTD_freeCStream(cs); return 1; }
        } while (rem != 0);
        comp_size = (unsigned)out.pos;
    }
    ZSTD_freeCStream(cs);

    ds = ZSTD_createDStream();
    if (!ds) { printf("  ZSTD_createDStream null\n"); return 1; }
    ZSTD_initDStream(ds);
    {
        ZSTD_inBuffer  in  = { comp, comp_size, 0 };
        ZSTD_outBuffer out = { deco, N, 0 };
        while (in.pos < in.size) {
            size_t r = ZSTD_decompressStream(ds, &out, &in);
            if (ZSTD_isError(r)) { printf("  zstd decompress error\n"); ZSTD_freeDStream(ds); return 1; }
            if (r == 0) break;
        }
        ok = (out.pos == N) && (memcmp(orig, deco, N) == 0);
    }
    ZSTD_freeDStream(ds);

    report("zstd", comp_size, ok);
    return ok ? 0 : 1;
}

/* ---------------------------------------------------------------- zlib ---- */

typedef struct {
    const unsigned char *next_in;
    unsigned int   avail_in;
    unsigned long  total_in;
    unsigned char *next_out;
    unsigned int   avail_out;
    unsigned long  total_out;
    const char    *msg;
    void          *state;
    void *(__stdcall *zalloc)(void *, unsigned, unsigned);
    void  (__stdcall *zfree)(void *, void *);
    void          *opaque;
    int            data_type;
    unsigned long  adler;
    unsigned long  reserved;
} z_stream;

/* deflateInit_ only checks the major version (first char), so "1" is enough
   and stays valid across zlib 1.x updates. */
#define ZLIB_VER "1"
#define Z_FINISH     4
#define Z_STREAM_END 1

typedef int (__stdcall *fn_deflateInit_)(z_stream *, int, const char *, int);
typedef int (__stdcall *fn_deflate)(z_stream *, int);
typedef int (__stdcall *fn_deflateEnd)(z_stream *);
typedef int (__stdcall *fn_inflateInit_)(z_stream *, const char *, int);
typedef int (__stdcall *fn_inflate)(z_stream *, int);
typedef int (__stdcall *fn_inflateEnd)(z_stream *);

static int test_zlib(const unsigned char *orig, unsigned char *comp, unsigned char *deco)
{
    HMODULE hc = LoadLibraryA("iszlib.dll");
    HMODULE hd = LoadLibraryA("isunzlib.dll");
    z_stream s;
    unsigned comp_size;
    int ok;

    if (!hc || !hd) { printf("  LoadLibrary failed (iszlib/isunzlib)\n"); return 1; }
    GET(hc, fn_deflateInit_, deflateInit_)
    GET(hc, fn_deflate,      deflate)
    GET(hc, fn_deflateEnd,   deflateEnd)
    GET(hd, fn_inflateInit_, inflateInit_)
    GET(hd, fn_inflate,      inflate)
    GET(hd, fn_inflateEnd,   inflateEnd)

    memset(&s, 0, sizeof s);
    s.zalloc = z_alloc; s.zfree = z_free;
    if (deflateInit_(&s, 6, ZLIB_VER, (int)sizeof(z_stream)) != 0) { printf("  deflateInit_ failed\n"); return 1; }
    s.next_in = orig; s.avail_in = N;
    s.next_out = comp; s.avail_out = COMPCAP;
    if (deflate(&s, Z_FINISH) != Z_STREAM_END) { printf("  deflate failed\n"); deflateEnd(&s); return 1; }
    comp_size = (unsigned)s.total_out;
    deflateEnd(&s);

    memset(&s, 0, sizeof s);
    s.zalloc = z_alloc; s.zfree = z_free;
    if (inflateInit_(&s, ZLIB_VER, (int)sizeof(z_stream)) != 0) { printf("  inflateInit_ failed\n"); return 1; }
    s.next_in = comp; s.avail_in = comp_size;
    s.next_out = deco; s.avail_out = N;
    if (inflate(&s, Z_FINISH) != Z_STREAM_END) { printf("  inflate failed\n"); inflateEnd(&s); return 1; }
    ok = (s.total_out == N) && (memcmp(orig, deco, N) == 0);
    inflateEnd(&s);

    report("zlib", comp_size, ok);
    return ok ? 0 : 1;
}

/* --------------------------------------------------------------- bzip2 ---- */

typedef struct {
    char *next_in;
    unsigned int avail_in;
    unsigned int total_in_lo32;
    unsigned int total_in_hi32;
    char *next_out;
    unsigned int avail_out;
    unsigned int total_out_lo32;
    unsigned int total_out_hi32;
    void *state;
    void *(__stdcall *bzalloc)(void *, int, int);
    void  (__stdcall *bzfree)(void *, void *);
    void *opaque;
} bz_stream;

#define BZ_FINISH     2
#define BZ_OK         0
#define BZ_FINISH_OK  3
#define BZ_STREAM_END 4

typedef int (__stdcall *fn_bzCompressInit)(bz_stream *, int, int, int);
typedef int (__stdcall *fn_bzCompress)(bz_stream *, int);
typedef int (__stdcall *fn_bzCompressEnd)(bz_stream *);
typedef int (__stdcall *fn_bzDecompressInit)(bz_stream *, int, int);
typedef int (__stdcall *fn_bzDecompress)(bz_stream *);
typedef int (__stdcall *fn_bzDecompressEnd)(bz_stream *);

static int test_bzip2(const unsigned char *orig, unsigned char *comp, unsigned char *deco)
{
    HMODULE hc = LoadLibraryA("isbzip.dll");
    HMODULE hd = LoadLibraryA("isbunzip.dll");
    bz_stream s;
    unsigned comp_size;
    int r, ok;

    if (!hc || !hd) { printf("  LoadLibrary failed (isbzip/isbunzip)\n"); return 1; }
    GET(hc, fn_bzCompressInit,   BZ2_bzCompressInit)
    GET(hc, fn_bzCompress,       BZ2_bzCompress)
    GET(hc, fn_bzCompressEnd,    BZ2_bzCompressEnd)
    GET(hd, fn_bzDecompressInit, BZ2_bzDecompressInit)
    GET(hd, fn_bzDecompress,     BZ2_bzDecompress)
    GET(hd, fn_bzDecompressEnd,  BZ2_bzDecompressEnd)

    memset(&s, 0, sizeof s);
    s.bzalloc = bz_alloc; s.bzfree = bz_free;
    if (BZ2_bzCompressInit(&s, 9, 0, 0) != BZ_OK) { printf("  BZ2_bzCompressInit failed\n"); return 1; }
    s.next_in = (char *)orig; s.avail_in = N;
    s.next_out = (char *)comp; s.avail_out = COMPCAP;
    do { r = BZ2_bzCompress(&s, BZ_FINISH); } while (r == BZ_FINISH_OK);
    if (r != BZ_STREAM_END) { printf("  BZ2_bzCompress failed (%d)\n", r); BZ2_bzCompressEnd(&s); return 1; }
    comp_size = s.total_out_lo32;
    BZ2_bzCompressEnd(&s);

    memset(&s, 0, sizeof s);
    s.bzalloc = bz_alloc; s.bzfree = bz_free;
    if (BZ2_bzDecompressInit(&s, 0, 0) != BZ_OK) { printf("  BZ2_bzDecompressInit failed\n"); return 1; }
    s.next_in = (char *)comp; s.avail_in = comp_size;
    s.next_out = (char *)deco; s.avail_out = N;
    do { r = BZ2_bzDecompress(&s); } while (r == BZ_OK);
    if (r != BZ_STREAM_END) { printf("  BZ2_bzDecompress failed (%d)\n", r); BZ2_bzDecompressEnd(&s); return 1; }
    ok = (s.total_out_lo32 == N) && (memcmp(orig, deco, N) == 0);
    BZ2_bzDecompressEnd(&s);

    report("bzip2", comp_size, ok);
    return ok ? 0 : 1;
}

/* ---------------------------------------------------------------- main ---- */

int main(void)
{
    unsigned char *orig = (unsigned char *)malloc(N);
    unsigned char *comp = (unsigned char *)malloc(COMPCAP);
    unsigned char *deco = (unsigned char *)malloc(N);
    int fails = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (!orig || !comp || !deco) { printf("buffer allocation failed\n"); return 2; }
    make_buffer(orig, N);

    fails += test_bzip2(orig, comp, deco);
    fails += test_zlib(orig, comp, deco);
    fails += test_zstd(orig, comp, deco);

    free(orig); free(comp); free(deco);

    printf("\n%s\n", fails ? "*** SOME TESTS FAILED ***" : "All round-trips OK");
    return fails ? 1 : 0;
}
