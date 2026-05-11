#include <windows.h>

#include "zutil.h"

/* see bzlib innosetup.c */
#pragma function(memset)
void * __cdecl memset(void *dst, int val, size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		((char *)dst)[i] = (char)val;
	}

	return dst;
}

void ZLIB_INTERNAL zmemcpy(void FAR *dest, const void FAR *source, z_size_t len) {
    uchf *d = dest;
    const uchf *s = source;
    if (len == 0) return;
    do {
        *d++ = *s++; /* ??? to be unrolled */
    } while (--len != 0);
}

void ZLIB_INTERNAL zmemzero(void FAR *dest, z_size_t len) {
    uchf *d = dest;
    if (len == 0) return;
    do {
        *d++ = 0;
    } while (--len != 0);
}

BOOL WINAPI _DllMainCRTStartup(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hInstance);
	}
	return TRUE;
}