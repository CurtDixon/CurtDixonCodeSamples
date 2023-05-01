#include "stdafx.h"
#include <stdlib.h>
#include "memdbg.h"
#include "kmemory.h"

using namespace BazisLib;

void * __cdecl operator new(size_t size)
{
#ifdef _DEBUG
	return MemoryDebug::OnMemoryAllocation(ExAllocatePoolWithTag(PagedPool, size + MemoryDebug::MEMORY_DEBUG_SHIFT, 'LACB'), size);
#else
	return ExAllocatePoolWithTag(PagedPool, size, 'WENB');
#endif
}

void * __cdecl npagednew(size_t size)
{
#ifdef _DEBUG
	return MemoryDebug::OnMemoryAllocation(ExAllocatePoolWithTag(NonPagedPool, size + MemoryDebug::MEMORY_DEBUG_SHIFT, 'LACB'), size);
#else
	return ExAllocatePoolWithTag(NonPagedPool, size, 'WENB');
#endif
}

void __cdecl operator delete(void *ptr)
{
	if (ptr != NULL)
	{
#ifdef _DEBUG
	ExFreePoolWithTag(MemoryDebug::OnMemoryFree(ptr), 'WENB');
#else
	ExFreePoolWithTag(ptr, 'WENB');
#endif
	}
}

extern "C" int _cdecl puts(const char *p);
int _cdecl puts(const char *p)
{
	return 0;
}

int atexit(void *p)
{
	return 0;
}

void * _cdecl malloc(size_t size)
{
#ifdef _DEBUG
	return MemoryDebug::OnMemoryAllocation(ExAllocatePoolWithTag(PagedPool, size + MemoryDebug::MEMORY_DEBUG_SHIFT, 'LACB'), size);
#else
	return ExAllocatePoolWithTag(PagedPool, size, 'LACB');
#endif
}

//void * __cdecl realloc(void * _Memory, size_t _Size)
void * __cdecl _realloc(void * _Memory, size_t _Size, size_t _OldSize)
{
	if (!_Memory)
		return malloc(_Size);
	if (!_Size)
	{
		free(_Memory);
		return NULL;
	}
	void *pMem = malloc(_Size);
	if (!pMem)
	{
		free(_Memory);
		return NULL;
	}
	memcpy(pMem, _Memory, min(_Size, _OldSize));
	free(_Memory);
	return pMem;
}


void _cdecl free(void *p)
{
#ifdef _DEBUG
	ExFreePoolWithTag(MemoryDebug::OnMemoryFree(p), 'LACB');
#else
	ExFreePoolWithTag(p, 'LACB');
#endif
}

void _cdecl exit(int _Code)
{
	KeBugCheck(STATUS_INTERNAL_ERROR);
}
