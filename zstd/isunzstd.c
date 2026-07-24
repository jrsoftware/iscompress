#include <windows.h>

void * __cdecl malloc(size_t size)
{
	return HeapAlloc(GetProcessHeap(), 0, size);
}

void __cdecl free(void *ptr)
{
	if (ptr != NULL)
		HeapFree(GetProcessHeap(), 0, ptr);
}

void * __cdecl calloc(size_t num, size_t size)
{
	if (size != 0 && num > (size_t)-1 / size)
		return NULL;
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, num * size);
}

/* see bzlib innosetup.c */
#pragma function(memset, memcpy, memmove)
void * __cdecl memset(void *dst, int val, size_t count)
{
	size_t i;

	for (i = 0; i < count; i++)
		((char *)dst)[i] = (char)val;

	return dst;
}

/* disable optimization so these copy loops aren't turned into calls to memcpy/memmove themselves */
#pragma optimize("", off)
void * __cdecl memcpy(void *dst, const void *src, size_t count)
{
	size_t i;

	for (i = 0; i < count; i++)
		((char *)dst)[i] = ((const char *)src)[i];

	return dst;
}

void * __cdecl memmove(void *dst, const void *src, size_t count)
{
	char *d = (char *)dst;
	const char *s = (const char *)src;

	if (d < s) {
		size_t i;
		for (i = 0; i < count; i++)
			d[i] = s[i];
	} else if (d > s) {
		size_t i = count;
		while (i-- != 0)
			d[i] = s[i];
	}

	return dst;
}
#pragma optimize("", on)

#if defined(_M_IX86)

/* EDX:EAX = low 64 bits of (A * B) */
__declspec(naked) void __cdecl _allmul(void)
{
	__asm {
		mov     eax, [esp+4]        ; A.lo
		mul     dword ptr [esp+16]  ; A.lo * B.hi
		mov     ecx, eax
		mov     eax, [esp+8]        ; A.hi
		mul     dword ptr [esp+12]  ; A.hi * B.lo
		add     ecx, eax
		mov     eax, [esp+4]        ; A.lo
		mul     dword ptr [esp+12]  ; A.lo * B.lo -> EDX:EAX
		add     edx, ecx
		ret     16
	}
}

/* EDX:EAX = EDX:EAX << CL */
__declspec(naked) void __cdecl _allshl(void)
{
	__asm {
		cmp     cl, 64
		jae     short retzero
		cmp     cl, 32
		jae     short more32
		shld    edx, eax, cl
		shl     eax, cl
		ret
	more32:
		mov     edx, eax
		xor     eax, eax
		and     cl, 31
		shl     edx, cl
		ret
	retzero:
		xor     eax, eax
		xor     edx, edx
		ret
	}
}

#endif

BOOL WINAPI _DllMainCRTStartup(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hInstance);
	}
	return TRUE;
}
