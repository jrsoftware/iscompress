#include <windows.h>

void bz_internal_error(int errcode)
{
	/* If an internal error is encountered, just throw an exception
	   with a random code. It'll probably leak memory, but that's
	   better than doing nothing, or killing the process. */
	RaiseException(0x06E15C8B, 0, 0, NULL);
}

void * __cdecl malloc(size_t size)
{
	return NULL;
}

void __cdecl free(void *ptr)
{
}

/* bzlib itself doesn't need memset, but VC2005's optimizer likes to replace
   assignment loops with calls to memset. */
#pragma function(memset)
void * __cdecl memset(void *dst, int val, size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		((char *)dst)[i] = (char)val;
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
