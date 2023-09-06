/*
	This file is part of libswirl
*/
//#include "license/bsd"


/*
    PVR-SB handling
    DMA hacks are here
*/

#include "types.h"
#include "pvr_regs.h"

bool pal_needs_update = true;
bool fog_needs_update = true;

u8 pvr_regs[pvr_RegSize];