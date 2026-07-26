#include <windows.h>

#undef RtlFillMemory

__declspec(dllimport) void RtlFillMemory(
   void*  Destination,
   size_t Length,
   int    Fill
);

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

/* bzlib itself doesn't need memset, but VS2022's optimizer likes to replace
   assignment loops with calls to memset. calling RtlFillMemory instead of
   looping is what makes whole program optimization (/GL) possible. this helper
   file is still compiled without /GL: /GL defers code generation to link time,
   where #pragma function may no longer be honored and memset could end up
   calling itself. */
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
