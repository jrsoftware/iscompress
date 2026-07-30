#include <windows.h>

#undef RtlFillMemory

__declspec(dllimport) void RtlFillMemory(
   void*  Destination,
   size_t Length,
   int    Fill
);

/* see bzlib innosetup.c */
#pragma function(memset)
void * __cdecl memset(void *dst, int val, size_t count)
{
	RtlFillMemory(dst, count, val);
	return dst;
}

BOOL WINAPI _DllMainCRTStartup(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hInstance);
	}
	return TRUE;
}