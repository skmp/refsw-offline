/*
	This file is part of libswirl
*/
#include "license/bsd"
#pragma once

#include "types.h"


// for TSP and TCW
#include "core_structs.h"




u32* raw_GetTexture(TSP tsp, TCW tcw);

void palette_update();

void BuildTwiddleTables();