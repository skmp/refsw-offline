/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "types.h"

#if BUILD_COMPILER==COMPILER_VC
#include <intrin.h>
#endif

u32 static INLINE bitscanrev(u32 v)
{
#if BUILD_COMPILER==COMPILER_GCC || BUILD_COMPILER==COMPILER_CLANG
	return 31-__builtin_clz(v);
#else
	unsigned long rv;
	_BitScanReverse(&rv,v);
	return rv;
#endif
}

//FIX ME
#define __assume(x)

void os_DebugBreak();
