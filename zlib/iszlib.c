#include <windows.h>

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

/* see bzlib innosetup.c */
#pragma function(memcpy)
void * __cdecl memcpy(void *dst, const void *src, size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		((char *)dst)[i] = ((char *)src)[i];
	}

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