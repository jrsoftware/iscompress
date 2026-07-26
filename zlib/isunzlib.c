#include <windows.h>

#include "zutil.h"

#undef RtlFillMemory
#undef RtlMoveMemory

__declspec(dllimport) void RtlFillMemory(
   void*  Destination,
   size_t Length,
   int    Fill
);

__declspec(dllimport) void RtlMoveMemory(
   void*       Destination,
   const void* Source,
   size_t      Length
);

/* see bzlib innosetup.c */
#pragma function(memset)
void * __cdecl memset(void *dst, int val, size_t count)
{
	RtlFillMemory(dst, count, val);
	return dst;
}

void ZLIB_INTERNAL zmemcpy(void FAR *dest, const void FAR *source, z_size_t len) {
    RtlMoveMemory(dest, source, len);
}

void ZLIB_INTERNAL zmemzero(void FAR *dest, z_size_t len) {
    RtlFillMemory(dest, len, 0);
}

BOOL WINAPI _DllMainCRTStartup(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hInstance);
	}
	return TRUE;
}