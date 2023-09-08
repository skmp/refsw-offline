/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "types.h"


u32 pvr_map32(u32 offset32);
f32 vrf(u8* vram, u32 addr);
u32 vri(u8* vram, u32 addr);
u32* vrp(u8* vram, u32 addr);

//vram 32-64b

//read
u16 pvr_read_area1_16(void*, u32 addr);
u32 pvr_read_area1_32(void*, u32 addr);
//write

void pvr_write_area1_16(void*, u32 addr,u16 data);
void pvr_write_area1_32(void*, u32 addr,u32 data);

//registers 
#define PVR_BASE 0x005F8000