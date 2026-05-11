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

void ZLIB_INTERNAL zmemcpy(Bytef* dest, const Bytef* source, z_size_t len) {
    if (len == 0) return;
    do {
        *dest++ = *source++; /* ??? to be unrolled */
    } while (--len != 0);
}

void ZLIB_INTERNAL zmemzero(Bytef* dest, z_size_t len) {
    if (len == 0) return;
    do {
        *dest++ = 0;
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