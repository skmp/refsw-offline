/*
	This file is part of libswirl
*/
#include "license/bsd"
#pragma once

#include "types.h"

struct text_info {
	u16* pdata;
	u32 width;
	u32 height;
	u32 textype; // 0 565, 1 1555, 2 4444
};

// for TSP and TCW
#include "core_structs.h"

text_info raw_GetTexture(u8* vram, TSP tsp, TCW tcw);

void palette_update();

void BuildTwiddleTables();