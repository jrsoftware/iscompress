#include <windows.h>
#include <VersionHelpers.h>

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

static INIT_ONCE g_InitOnce = INIT_ONCE_STATIC_INIT;
typedef void (* MEMCPYPROC)(void*,const void*,size_t);
static MEMCPYPROC __RtlCopyMemory = (MEMCPYPROC)RtlMoveMemory;

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

#pragma function(memset, memcpy, memmove)
void * __cdecl memset(void *dst, int val, size_t count)
{
	RtlFillMemory(dst, count, val);
	return dst;
}

void * __cdecl memcpy(void *dst, const void *src, size_t count)
{
	__RtlCopyMemory(dst, src, count);
	return dst;
}

void * __cdecl memmove(void *dst, const void *src, size_t count)
{
	RtlMoveMemory(dst, src, count);
	return dst;
}

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

static BOOL CALLBACK GetRtlCopyMemory(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* Context)
{
	if (!IsWindows7SP1OrGreater()) {
		// RtlMoveMemory already set as a fallback
		return TRUE;
	}

	HMODULE hHandle = GetModuleHandle(TEXT("kernel32.dll"));
	MEMCPYPROC f = NULL;
	if (hHandle) {
		f = (MEMCPYPROC)GetProcAddress(hHandle, "RtlCopyMemory");
	}
	if (f != NULL) {
		__RtlCopyMemory = f;
	}
	return TRUE;
}

BOOL WINAPI _DllMainCRTStartup(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hInstance);
	}
	InitOnceExecuteOnce(&g_InitOnce, GetRtlCopyMemory, NULL, NULL);
	return TRUE;
}
