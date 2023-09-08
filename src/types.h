/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "build.h"


//includes from CRT
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <cstring>

//includes from c++rt
#include <vector>
#include <string>
#include <map>


//basic types
typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef ptrdiff_t snat;
typedef uintptr_t unat;


using namespace std;

//basic includes
//#include "stdclass.h"

#include "oslib.h"


#define verify(x) if((x)==false){ printf("Verify Failed  : " #x "\n in %s -> %s : %d \n",(__FUNCTION__),(__FILE__),__LINE__); os_DebugBreak();}
#define die(reason) { printf("Fatal error : %s\n in %s -> %s : %d \n",(reason),(__FUNCTION__),(__FILE__),__LINE__); os_DebugBreak();}

#define fverify verify

struct settings_t
{
	// ?
};

extern settings_t settings;

inline bool is_s8(u32 v) { return (s8)v==(s32)v; }
inline bool is_u8(u32 v) { return (u8)v==(s32)v; }
inline bool is_s16(u32 v) { return (s16)v==(s32)v; }
inline bool is_u16(u32 v) { return (u16)v==(u32)v; }

struct OnLoad
{
	typedef void OnLoadFP();
	OnLoad(OnLoadFP* fp) { fp(); }
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

