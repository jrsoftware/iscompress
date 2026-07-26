/*
 * Round-trip and speed test for the Inno Setup compression DLLs.
 *
 * For each codec it loads the compression and decompression DLLs with
 * LoadLibrary/GetProcAddress, compresses a test buffer, decompresses the
 * result, and checks it matches the original bytes.
 *
 * This runs in two phases. The round-trip phase uses a small buffer, so a
 * missing export or a broken DLL is reported within a second. Only if every
 * codec passes does the speed phase run: it raises the process to high
 * priority and times compression and decompression of a much larger buffer,
 * reporting MB/s (and still verifying the data, which is free).
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

#define N        200000u                  /* round-trip buffer size */
#define SPEED_N  (16u * 1000u * 1000u)    /* speed test buffer size */
#define MB       1000000.0                /* decimal, as zstd's own benchmark
                                             reports speeds (its MB_UNIT) */
#define MIN_PASS 3                        /* never time fewer passes than this */
#define MIN_SECS 0.5                      /* sample for at least this long in total */
#define MAX_SECS 3.0                      /* ... but no longer, once MIN_PASS is met */
#define SETTLED  2                        /* passes the best must survive to be believed */

/* Compressed-output capacity, above the worst case every codec documents for
   incompressible input: bzip2 wants 1% over the input plus 600 bytes,
   ZSTD_compressBound about 0.4%, and zlib's compressBound about 0.03%. Two
   percent plus 4 KB clears all three, so a change to make_buffer can never
   fail the test by producing data that does not compress. */
#define COMPCAP  (SPEED_N + SPEED_N / 50u + 4096u)

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

/* Resolve an export into a function pointer variable whose name differs from
   it - needed where both DLLs export the same function under the same name. */
#define GET_AS(h, type, var, name) \
    var = (type)GetProcAddress(h, name); \
    if (!var) { printf("  missing export %s\n", name); return 1; }

/* Resolve an export into the like-named function pointer variable. */
#define GET(h, type, fn) GET_AS(h, type, fn, #fn)

/* One compression or decompression pass, self-contained: it creates its own
   stream state, so it can be called repeatedly for timing. Returns 0 on
   success, 1 after printing what went wrong. */
typedef int (*fn_pass)(const unsigned char *src, size_t src_size,
                       unsigned char *dst, size_t dst_cap, size_t *dst_size);

typedef struct {
    const char *name;
    const char *comp_dll;
    const char *deco_dll;
    int (*load)(HMODULE hc, HMODULE hd);
    fn_pass compress;
    fn_pass decompress;
} codec;

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

static fn_createCStream    ZSTD_createCStream;
static fn_initCStream      ZSTD_initCStream;
static fn_compressStream2  ZSTD_compressStream2;
static fn_freeCStream      ZSTD_freeCStream;
static fn_createDStream    ZSTD_createDStream;
static fn_initDStream      ZSTD_initDStream;
static fn_decompressStream ZSTD_decompressStream;
static fn_freeDStream      ZSTD_freeDStream;

/* Both DLLs export ZSTD_isError, so each copy gets resolved and used by the DLL
   it came from. Sharing one would leave isunzstd's export untested. */
static fn_isError          ZSTD_isError_iszstd;
static fn_isError          ZSTD_isError_isunzstd;

static int zstd_load(HMODULE hc, HMODULE hd)
{
    GET(hc, fn_createCStream,    ZSTD_createCStream)
    GET(hc, fn_initCStream,      ZSTD_initCStream)
    GET(hc, fn_compressStream2,  ZSTD_compressStream2)
    GET(hc, fn_freeCStream,      ZSTD_freeCStream)
    GET_AS(hc, fn_isError,       ZSTD_isError_iszstd, "ZSTD_isError")
    GET(hd, fn_createDStream,    ZSTD_createDStream)
    GET(hd, fn_initDStream,      ZSTD_initDStream)
    GET(hd, fn_decompressStream, ZSTD_decompressStream)
    GET(hd, fn_freeDStream,      ZSTD_freeDStream)
    GET_AS(hd, fn_isError,       ZSTD_isError_isunzstd, "ZSTD_isError")
    return 0;
}

static int zstd_compress(const unsigned char *src, size_t src_size,
                         unsigned char *dst, size_t dst_cap, size_t *dst_size)
{
    ZSTD_inBuffer  in  = { src, src_size, 0 };
    ZSTD_outBuffer out = { dst, dst_cap, 0 };
    void *cs = ZSTD_createCStream();
    size_t rem;

    if (!cs) { printf("  ZSTD_createCStream null\n"); return 1; }
    ZSTD_initCStream(cs, 6);
    do {
        rem = ZSTD_compressStream2(cs, &out, &in, /*ZSTD_e_end*/2);
        if (ZSTD_isError_iszstd(rem)) { printf("  zstd compress error\n"); ZSTD_freeCStream(cs); return 1; }
    } while (rem != 0);
    ZSTD_freeCStream(cs);

    *dst_size = out.pos;
    return 0;
}

static int zstd_decompress(const unsigned char *src, size_t src_size,
                           unsigned char *dst, size_t dst_cap, size_t *dst_size)
{
    ZSTD_inBuffer  in  = { src, src_size, 0 };
    ZSTD_outBuffer out = { dst, dst_cap, 0 };
    void *ds = ZSTD_createDStream();
    size_t r = 1; /* nonzero until a frame is completely decoded and flushed */

    if (!ds) { printf("  ZSTD_createDStream null\n"); return 1; }
    ZSTD_initDStream(ds);
    while (in.pos < in.size) {
        r = ZSTD_decompressStream(ds, &out, &in);
        if (ZSTD_isError_isunzstd(r)) { printf("  zstd decompress error\n"); ZSTD_freeDStream(ds); return 1; }
        if (r == 0) break;
    }
    ZSTD_freeDStream(ds);

    /* r==0 with all input consumed proves the frame ended cleanly, not that
       the output merely happened to reach its expected size (a truncated frame
       must fail). */
    if (r != 0 || in.pos != in.size) { printf("  zstd frame did not end cleanly\n"); return 1; }

    *dst_size = out.pos;
    return 0;
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

static fn_deflateInit_ deflateInit_;
static fn_deflate      deflate;
static fn_deflateEnd   deflateEnd;
static fn_inflateInit_ inflateInit_;
static fn_inflate      inflate;
static fn_inflateEnd   inflateEnd;

static int zlib_load(HMODULE hc, HMODULE hd)
{
    GET(hc, fn_deflateInit_, deflateInit_)
    GET(hc, fn_deflate,      deflate)
    GET(hc, fn_deflateEnd,   deflateEnd)
    GET(hd, fn_inflateInit_, inflateInit_)
    GET(hd, fn_inflate,      inflate)
    GET(hd, fn_inflateEnd,   inflateEnd)
    return 0;
}

static int zlib_compress(const unsigned char *src, size_t src_size,
                         unsigned char *dst, size_t dst_cap, size_t *dst_size)
{
    z_stream s;

    memset(&s, 0, sizeof s);
    s.zalloc = z_alloc; s.zfree = z_free;
    if (deflateInit_(&s, 6, ZLIB_VER, (int)sizeof(z_stream)) != 0) { printf("  deflateInit_ failed\n"); return 1; }
    s.next_in = src; s.avail_in = (unsigned int)src_size;
    s.next_out = dst; s.avail_out = (unsigned int)dst_cap;
    if (deflate(&s, Z_FINISH) != Z_STREAM_END) { printf("  deflate failed\n"); deflateEnd(&s); return 1; }
    *dst_size = s.total_out;
    deflateEnd(&s);
    return 0;
}

static int zlib_decompress(const unsigned char *src, size_t src_size,
                           unsigned char *dst, size_t dst_cap, size_t *dst_size)
{
    z_stream s;

    memset(&s, 0, sizeof s);
    s.zalloc = z_alloc; s.zfree = z_free;
    if (inflateInit_(&s, ZLIB_VER, (int)sizeof(z_stream)) != 0) { printf("  inflateInit_ failed\n"); return 1; }
    s.next_in = src; s.avail_in = (unsigned int)src_size;
    s.next_out = dst; s.avail_out = (unsigned int)dst_cap;
    if (inflate(&s, Z_FINISH) != Z_STREAM_END) { printf("  inflate failed\n"); inflateEnd(&s); return 1; }
    *dst_size = s.total_out;
    inflateEnd(&s);
    return 0;
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

static fn_bzCompressInit   BZ2_bzCompressInit;
static fn_bzCompress       BZ2_bzCompress;
static fn_bzCompressEnd    BZ2_bzCompressEnd;
static fn_bzDecompressInit BZ2_bzDecompressInit;
static fn_bzDecompress     BZ2_bzDecompress;
static fn_bzDecompressEnd  BZ2_bzDecompressEnd;

static int bzip2_load(HMODULE hc, HMODULE hd)
{
    GET(hc, fn_bzCompressInit,   BZ2_bzCompressInit)
    GET(hc, fn_bzCompress,       BZ2_bzCompress)
    GET(hc, fn_bzCompressEnd,    BZ2_bzCompressEnd)
    GET(hd, fn_bzDecompressInit, BZ2_bzDecompressInit)
    GET(hd, fn_bzDecompress,     BZ2_bzDecompress)
    GET(hd, fn_bzDecompressEnd,  BZ2_bzDecompressEnd)
    return 0;
}

static int bzip2_compress(const unsigned char *src, size_t src_size,
                          unsigned char *dst, size_t dst_cap, size_t *dst_size)
{
    bz_stream s;
    int r;

    memset(&s, 0, sizeof s);
    s.bzalloc = bz_alloc; s.bzfree = bz_free;
    if (BZ2_bzCompressInit(&s, 9, 0, 0) != BZ_OK) { printf("  BZ2_bzCompressInit failed\n"); return 1; }
    s.next_in = (char *)src; s.avail_in = (unsigned int)src_size;
    s.next_out = (char *)dst; s.avail_out = (unsigned int)dst_cap;
    do { r = BZ2_bzCompress(&s, BZ_FINISH); } while (r == BZ_FINISH_OK);
    if (r != BZ_STREAM_END) { printf("  BZ2_bzCompress failed (%d)\n", r); BZ2_bzCompressEnd(&s); return 1; }
    *dst_size = s.total_out_lo32;
    BZ2_bzCompressEnd(&s);
    return 0;
}

static int bzip2_decompress(const unsigned char *src, size_t src_size,
                            unsigned char *dst, size_t dst_cap, size_t *dst_size)
{
    bz_stream s;
    int r;

    memset(&s, 0, sizeof s);
    s.bzalloc = bz_alloc; s.bzfree = bz_free;
    if (BZ2_bzDecompressInit(&s, 0, 0) != BZ_OK) { printf("  BZ2_bzDecompressInit failed\n"); return 1; }
    s.next_in = (char *)src; s.avail_in = (unsigned int)src_size;
    s.next_out = (char *)dst; s.avail_out = (unsigned int)dst_cap;
    do { r = BZ2_bzDecompress(&s); } while (r == BZ_OK);
    if (r != BZ_STREAM_END) { printf("  BZ2_bzDecompress failed (%d)\n", r); BZ2_bzDecompressEnd(&s); return 1; }
    *dst_size = s.total_out_lo32;
    BZ2_bzDecompressEnd(&s);
    return 0;
}

/* --------------------------------------------------------------- codecs ---- */

static const codec codecs[] = {
    { "bzip2", "isbzip.dll", "isbunzip.dll", bzip2_load, bzip2_compress, bzip2_decompress },
    { "zlib",  "iszlib.dll", "isunzlib.dll", zlib_load,  zlib_compress,  zlib_decompress  },
    { "zstd",  "iszstd.dll", "isunzstd.dll", zstd_load,  zstd_compress,  zstd_decompress  }
};
#define NCODECS (sizeof codecs / sizeof codecs[0])

static int load_codec(const codec *c)
{
    HMODULE hc = LoadLibraryA(c->comp_dll);
    HMODULE hd = LoadLibraryA(c->deco_dll);

    if (!hc || !hd) {
        printf("  LoadLibrary failed (%s/%s)\n", c->comp_dll, c->deco_dll);
        return 1;
    }
    return c->load(hc, hd);
}

/* ---------------------------------------------------------- round-trip ---- */

static int roundtrip(const codec *c, const unsigned char *orig,
                     unsigned char *comp, unsigned char *deco)
{
    size_t comp_size, deco_size;
    int ok;

    if (c->compress(orig, N, comp, COMPCAP, &comp_size)) return 1;
    if (c->decompress(comp, comp_size, deco, N, &deco_size)) return 1;

    ok = (deco_size == N) && (memcmp(orig, deco, N) == 0);
    printf("  %-6s orig=%u compress=%u decompress=%u %s\n",
           c->name, (unsigned)N, (unsigned)comp_size, (unsigned)deco_size,
           ok ? "OK" : "FAIL");
    return ok ? 0 : 1;
}

/* --------------------------------------------------------------- speed ---- */

static double seconds_now(void)
{
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}

/* Time one pass repeatedly and keep the fastest. The fastest is the
   reproducible number: everything that can perturb a pass - another process
   taking the core or the memory bandwidth, the scheduler parking us on an
   efficiency core, a missed turbo opportunity - only ever makes it slower, so
   averaging the passes measures how busy the machine was rather than how fast
   the DLL is.

   Sampling stops once MIN_PASS passes have run, the best has survived SETTLED
   further passes, and MIN_SECS have elapsed. That keeps a clean run short
   while automatically sampling longer through a burst of background load (a
   virus scanner reading the just-compiled EXE, say), which otherwise skews
   whichever codec happens to be running at the time. MAX_SECS bounds the wait
   if the load never lets up - the numbers are then simply what this machine
   can do while busy. */
static int measure(fn_pass pass, const unsigned char *src, size_t src_size,
                   unsigned char *dst, size_t dst_cap, size_t *dst_size,
                   double *secs)
{
    double start = seconds_now(), best = 0.0, total;
    unsigned passes = 0, since_best = 0;

    for (;;) {
        double t0 = seconds_now(), elapsed;
        if (pass(src, src_size, dst, dst_cap, dst_size)) return 1;
        elapsed = seconds_now() - t0;
        if (passes == 0 || elapsed < best) { best = elapsed; since_best = 0; }
        else since_best++;
        passes++;

        total = seconds_now() - start;
        if (passes < MIN_PASS) continue;
        if (total >= MAX_SECS) break;
        if (total >= MIN_SECS && since_best >= SETTLED) break;
    }

    *secs = best;
    return 0;
}

static int speed(const codec *c, const unsigned char *orig,
                 unsigned char *comp, unsigned char *deco)
{
    size_t comp_size, deco_size;
    double comp_secs, deco_secs;

    if (measure(c->compress, orig, SPEED_N, comp, COMPCAP, &comp_size, &comp_secs)) return 1;
    if (measure(c->decompress, comp, comp_size, deco, SPEED_N, &deco_size, &deco_secs)) return 1;

    if (deco_size != SPEED_N || memcmp(orig, deco, SPEED_N) != 0) {
        printf("  %-6s data mismatch\n", c->name);
        return 1;
    }

    /* Both rates are uncompressed bytes per second, the usual convention. */
    printf("  %-6s ratio=%.2f compress=%7.1f MB/s decompress=%7.1f MB/s\n",
           c->name, (double)SPEED_N / comp_size,
           SPEED_N / MB / comp_secs, SPEED_N / MB / deco_secs);
    return 0;
}

/* ---------------------------------------------------------------- main ---- */

int main(void)
{
    unsigned char *orig = (unsigned char *)malloc(SPEED_N);
    unsigned char *comp = (unsigned char *)malloc(COMPCAP);
    unsigned char *deco = (unsigned char *)malloc(SPEED_N);
    size_t i;
    int fails = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (!orig || !comp || !deco) { printf("buffer allocation failed\n"); return 2; }
    make_buffer(orig, SPEED_N);

    for (i = 0; i < NCODECS; i++)
        fails += load_codec(&codecs[i]);
    if (fails) { printf("\n*** DLLS COULD NOT BE LOADED ***\n"); return 1; }

    printf("Round-trips (%u bytes)\n", (unsigned)N);
    for (i = 0; i < NCODECS; i++)
        fails += roundtrip(&codecs[i], orig, comp, deco);
    if (fails) { printf("\n*** SOME ROUND-TRIPS FAILED ***\n"); return 1; }

    printf("\nSpeed (%u MB, high priority)\n", (unsigned)(SPEED_N / 1000000u));
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        printf("  note: could not raise process priority (%lu)\n", GetLastError());
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    for (i = 0; i < NCODECS; i++)
        fails += speed(&codecs[i], orig, comp, deco);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

    free(orig); free(comp); free(deco);

    printf("\n%s\n", fails ? "*** SOME SPEED TESTS FAILED ***" : "All tests OK");
    return fails ? 1 : 0;
}
