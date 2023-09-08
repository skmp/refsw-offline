/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "types.h"
#include "pvr_regs.h"
#include "TexCache.h"

u8* vq_codebook;
u32 palette_index;

u32 palette16_ram[1024];
u32 palette32_ram[1024];

u32 detwiddle[2][8][1024];
//input : address in the yyyyyxxxxx format
//output : address in the xyxyxyxy format
//U : x resolution , V : y resolution
//twiddle works on 64b words


u32 twiddle_slow(u32 x,u32 y,u32 x_sz,u32 y_sz)
{
	u32 rv=0;//low 2 bits are directly passed  -> needs some misc stuff to work.However
			 //Pvr internally maps the 64b banks "as if" they were twiddled :p

	u32 sh=0;
	x_sz>>=1;
	y_sz>>=1;
	while(x_sz!=0 || y_sz!=0)
	{
		if (y_sz)
		{
			u32 temp=y&1;
			rv|=temp<<sh;

			y_sz>>=1;
			y>>=1;
			sh++;
		}
		if (x_sz)
		{
			u32 temp=x&1;
			rv|=temp<<sh;

			x_sz>>=1;
			x>>=1;
			sh++;
		}
	}	
	return rv;
}

void BuildTwiddleTables()
{
	for (u32 s=0;s<8;s++)
	{
		u32 x_sz=1024;
		u32 y_sz=8<<s;
		for (u32 i=0;i<x_sz;i++)
		{
			detwiddle[0][s][i]=twiddle_slow(i,0,x_sz,y_sz);
			detwiddle[1][s][i]=twiddle_slow(0,i,y_sz,x_sz);
		}
	}
}

static OnLoad btt(&BuildTwiddleTables);

void palette_update()
{
	switch(PAL_RAM_CTRL&3)
	{
	case 0:
		for (int i=0;i<1024;i++)
		{
			palette32_ram[i] = ARGB1555_32(PALETTE_RAM[i]);
		}
		break;

	case 1:
		for (int i=0;i<1024;i++)
		{
			palette32_ram[i] = ARGB565_32(PALETTE_RAM[i]);
		}
		break;

	case 2:
		for (int i=0;i<1024;i++)
		{
			palette32_ram[i] = ARGB4444_32(PALETTE_RAM[i]);
		}
		break;

	case 3:
		for (int i=0;i<1024;i++)
		{
			palette32_ram[i] = ARGB8888_32(PALETTE_RAM[i]);
		}
		break;
	}
}