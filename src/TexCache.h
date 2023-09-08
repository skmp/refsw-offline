/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "types.h"
#include "oslib.h"

extern u8* vq_codebook;
extern u32 palette_index;
extern u32 palette32_ram[1024];

extern u32 detwiddle[2][8][1024];

template<class pixel_type>
class PixelBuffer
{
	pixel_type* p_buffer_start;
	pixel_type* p_current_line;
	pixel_type* p_current_pixel;
	u32 pixels_per_line;

public:
	u32 total_pixels;
	

	PixelBuffer()
	{
		p_buffer_start = p_current_line = p_current_pixel = NULL;
	}

	~PixelBuffer()
	{
		deinit();
	}

	void init(u32 width, u32 height)
	{
		deinit();
		p_buffer_start = p_current_line = p_current_pixel = (pixel_type *)malloc(width * height * sizeof(pixel_type));
		this->total_pixels = width * height;
		this->pixels_per_line = width;
	}

	void deinit()
	{
		if (p_buffer_start != NULL)
		{
			free(p_buffer_start);
			p_buffer_start = p_current_line = p_current_pixel = NULL;
		}
	}

	void steal_data(PixelBuffer &buffer)
	{
		deinit();
		p_buffer_start = p_current_line = p_current_pixel = buffer.p_buffer_start;
		pixels_per_line = buffer.pixels_per_line;
		buffer.p_buffer_start = buffer.p_current_line = buffer.p_current_pixel = NULL;
	}

	pixel_type *data(u32 x = 0, u32 y = 0)
	{
		return p_buffer_start + pixels_per_line * y + x;
	}

	void prel(u32 x,pixel_type value)
	{
		p_current_pixel[x]=value;
	}

	void prel(u32 x,u32 y,pixel_type value)
	{
		p_current_pixel[y*pixels_per_line+x]=value;
	}

	void rmovex(u32 value)
	{
		p_current_pixel+=value;
	}
	void rmovey(u32 value)
	{
		p_current_line+=pixels_per_line*value;
		p_current_pixel=p_current_line;
	}
	void amove(u32 x_m,u32 y_m)
	{
		//p_current_pixel=p_buffer_start;
		p_current_line=p_buffer_start+pixels_per_line*y_m;
		p_current_pixel=p_current_line + x_m;
	}
};

void palette_update();

template<class pixel_type>
pixel_type cclamp(pixel_type minv, pixel_type maxv, pixel_type x) {
	return std::min(maxv, std::max(minv, x));
}

// Unpack to 32-bit word

#define ARGB1555_32( word )    ( ((word & 0x8000) ? 0xFF000000 : 0) | (((word>>0) & 0x1F)<<3)  | (((word>>5) & 0x1F)<<11)  | (((word>>10) & 0x1F)<<19) )

#define ARGB565_32( word )     ( (((word>>0)&0x1F)<<3) | (((word>>5)&0x3F)<<10) | (((word>>11)&0x1F)<<19) | 0xFF000000 )

#define ARGB4444_32( word ) ( (((word>>12)&0xF)<<28) | (((word>>0)&0xF)<<4) | (((word>>4)&0xF)<<12) | (((word>>8)&0xF)<<20) )

#define ARGB8888_32( word ) ( ((word >> 0) & 0xFF000000) | (((word >> 0) & 0xFF) << 0) | (((word >> 8) & 0xFF) << 8) | (((word >> 16) & 0xFF) << 16) )

static u32 packRGB(u8 R,u8 G,u8 B)
{
	return (R << 0) | (G << 8) | (B << 16) | 0xFF000000;
}

static u32 YUV422(s32 Y,s32 Yu,s32 Yv)
{
	Yu-=128;
	Yv-=128;

	//s32 B = (76283*(Y - 16) + 132252*(Yu - 128))>>16;
	//s32 G = (76283*(Y - 16) - 53281 *(Yv - 128) - 25624*(Yu - 128))>>16;
	//s32 R = (76283*(Y - 16) + 104595*(Yv - 128))>>16;
	
	s32 R = Y + Yv*11/8;            // Y + (Yv-128) * (11/8) ?
	s32 G = Y - (Yu*11 + Yv*22)/32; // Y - (Yu-128) * (11/8) * 0.25 - (Yv-128) * (11/8) * 0.5 ?
	s32 B = Y + Yu*110/64;          // Y + (Yu-128) * (11/8) * 1.25 ?

	return packRGB(cclamp<s32>(0, 255, R),cclamp<s32>(0, 255, G),cclamp<s32>(0, 255, B));
}

#define twop(x,y,bcx,bcy) (detwiddle[0][bcy][x]+detwiddle[1][bcx][y])


//pixel convertors !
#define pixelcvt_start_base(name,x,y,type) \
		struct name \
		{ \
			static const u32 xpp=x;\
			static const u32 ypp=y;	\
			static void Convert(PixelBuffer<type>* pb,u8* data) \
		{

#define pixelcvt_start(name,x,y) pixelcvt_start_base(name, x, y, u16)
#define pixelcvt32_start(name,x,y) pixelcvt_start_base(name, x, y, u32)

#define pixelcvt_end } }
#define pixelcvt_next(name,x,y) pixelcvt_end;  pixelcvt_start(name,x,y)
//
//Non twiddled
//

// 32-bit pixel buffer
pixelcvt32_start(conv565_PL32,4,1)
{
	//convert 4x1
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,ARGB565_32(p_in[0]));
	//1,0
	pb->prel(1,ARGB565_32(p_in[1]));
	//2,0
	pb->prel(2,ARGB565_32(p_in[2]));
	//3,0
	pb->prel(3,ARGB565_32(p_in[3]));
}
pixelcvt_end;
pixelcvt32_start(conv1555_PL32,4,1)
{
	//convert 4x1
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,ARGB1555_32(p_in[0]));
	//1,0
	pb->prel(1,ARGB1555_32(p_in[1]));
	//2,0
	pb->prel(2,ARGB1555_32(p_in[2]));
	//3,0
	pb->prel(3,ARGB1555_32(p_in[3]));
}
pixelcvt_end;
pixelcvt32_start(conv4444_PL32,4,1)
{
	//convert 4x1
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,ARGB4444_32(p_in[0]));
	//1,0
	pb->prel(1,ARGB4444_32(p_in[1]));
	//2,0
	pb->prel(2,ARGB4444_32(p_in[2]));
	//3,0
	pb->prel(3,ARGB4444_32(p_in[3]));
}
pixelcvt_end;
pixelcvt32_start(convYUV_PL,4,1)
{
	//convert 4x1 4444 to 4x1 8888
	u32* p_in=(u32*)data;


	s32 Y0 = (p_in[0]>>8) &255; //
	s32 Yu = (p_in[0]>>0) &255; //p_in[0]
	s32 Y1 = (p_in[0]>>24) &255; //p_in[3]
	s32 Yv = (p_in[0]>>16) &255; //p_in[2]

	//0,0
	pb->prel(0,YUV422(Y0,Yu,Yv));
	//1,0
	pb->prel(1,YUV422(Y1,Yu,Yv));

	//next 4 bytes
	p_in+=1;

	Y0 = (p_in[0]>>8) &255; //
	Yu = (p_in[0]>>0) &255; //p_in[0]
	Y1 = (p_in[0]>>24) &255; //p_in[3]
	Yv = (p_in[0]>>16) &255; //p_in[2]

	//0,0
	pb->prel(2,YUV422(Y0,Yu,Yv));
	//1,0
	pb->prel(3,YUV422(Y1,Yu,Yv));
}
pixelcvt_end;

//
//twiddled
//

// 32-bit pixel buffer
pixelcvt32_start(conv565_TW32,2,2)
{
	//convert 4x1 565 to 4x1 8888
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,0,ARGB565_32(p_in[0]));
	//0,1
	pb->prel(0,1,ARGB565_32(p_in[1]));
	//1,0
	pb->prel(1,0,ARGB565_32(p_in[2]));
	//1,1
	pb->prel(1,1,ARGB565_32(p_in[3]));
}
pixelcvt_end;
pixelcvt32_start(conv1555_TW32,2,2)
{
	//convert 4x1 565 to 4x1 8888
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,0,ARGB1555_32(p_in[0]));
	//0,1
	pb->prel(0,1,ARGB1555_32(p_in[1]));
	//1,0
	pb->prel(1,0,ARGB1555_32(p_in[2]));
	//1,1
	pb->prel(1,1,ARGB1555_32(p_in[3]));
}
pixelcvt_end;
pixelcvt32_start(conv4444_TW32,2,2)
{
	//convert 4x1 565 to 4x1 8888
	u16* p_in=(u16*)data;
	//0,0
	pb->prel(0,0,ARGB4444_32(p_in[0]));
	//0,1
	pb->prel(0,1,ARGB4444_32(p_in[1]));
	//1,0
	pb->prel(1,0,ARGB4444_32(p_in[2]));
	//1,1
	pb->prel(1,1,ARGB4444_32(p_in[3]));
}
pixelcvt_end;

pixelcvt32_start(convYUV_TW,2,2)
{
	//convert 4x1 4444 to 4x1 8888
	u16* p_in=(u16*)data;


	s32 Y0 = (p_in[0]>>8) &255; //
	s32 Yu = (p_in[0]>>0) &255; //p_in[0]
	s32 Y1 = (p_in[2]>>8) &255; //p_in[3]
	s32 Yv = (p_in[2]>>0) &255; //p_in[2]

	//0,0
	pb->prel(0,0,YUV422(Y0,Yu,Yv));
	//1,0
	pb->prel(1,0,YUV422(Y1,Yu,Yv));

	//next 4 bytes
	//p_in+=2;

	Y0 = (p_in[1]>>8) &255; //
	Yu = (p_in[1]>>0) &255; //p_in[0]
	Y1 = (p_in[3]>>8) &255; //p_in[3]
	Yv = (p_in[3]>>0) &255; //p_in[2]

	//0,1
	pb->prel(0,1,YUV422(Y0,Yu,Yv));
	//1,1
	pb->prel(1,1,YUV422(Y1,Yu,Yv));
}
pixelcvt_end;

// 16-bit && 32-bit pixel buffers
pixelcvt32_start(convPAL4_TW,4,4)
{
	u8* p_in=(u8*)data;
	u32* pal= &palette32_ram[palette_index];

	pb->prel(0,0,pal[p_in[0]&0xF]);
	pb->prel(0,1,pal[(p_in[0]>>4)&0xF]);p_in++;
	pb->prel(1,0,pal[p_in[0]&0xF]);
	pb->prel(1,1,pal[(p_in[0]>>4)&0xF]);p_in++;

	pb->prel(0,2,pal[p_in[0]&0xF]);
	pb->prel(0,3,pal[(p_in[0]>>4)&0xF]);p_in++;
	pb->prel(1,2,pal[p_in[0]&0xF]);
	pb->prel(1,3,pal[(p_in[0]>>4)&0xF]);p_in++;

	pb->prel(2,0,pal[p_in[0]&0xF]);
	pb->prel(2,1,pal[(p_in[0]>>4)&0xF]);p_in++;
	pb->prel(3,0,pal[p_in[0]&0xF]);
	pb->prel(3,1,pal[(p_in[0]>>4)&0xF]);p_in++;

	pb->prel(2,2,pal[p_in[0]&0xF]);
	pb->prel(2,3,pal[(p_in[0]>>4)&0xF]);p_in++;
	pb->prel(3,2,pal[p_in[0]&0xF]);
	pb->prel(3,3,pal[(p_in[0]>>4)&0xF]);p_in++;
}
pixelcvt_end;

pixelcvt32_start(convPAL8_TW,2,4)
{
	u8* p_in=(u8*)data;
	u32* pal= &palette32_ram[palette_index];

	pb->prel(0,0,pal[p_in[0]]);p_in++;
	pb->prel(0,1,pal[p_in[0]]);p_in++;
	pb->prel(1,0,pal[p_in[0]]);p_in++;
	pb->prel(1,1,pal[p_in[0]]);p_in++;

	pb->prel(0,2,pal[p_in[0]]);p_in++;
	pb->prel(0,3,pal[p_in[0]]);p_in++;
	pb->prel(1,2,pal[p_in[0]]);p_in++;
	pb->prel(1,3,pal[p_in[0]]);p_in++;
}
pixelcvt_end;

//handler functions
template<class PixelConvertor, class pixel_type>
void texture_PL(PixelBuffer<pixel_type>* pb,u8* p_in,u32 Width,u32 Height)
{
	pb->amove(0,0);

	Height/=PixelConvertor::ypp;
	Width/=PixelConvertor::xpp;

	for (u32 y=0;y<Height;y++)
	{
		for (u32 x=0;x<Width;x++)
		{
			u8* p = p_in;
			PixelConvertor::Convert(pb,p);
			p_in+=8;

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

template<class PixelConvertor, class pixel_type>
void texture_TW(PixelBuffer<pixel_type>* pb,u8* p_in,u32 Width,u32 Height)
{
	pb->amove(0,0);

	const u32 divider=PixelConvertor::xpp*PixelConvertor::ypp;

	u32 bcx_,bcy_;
	bcx_=bitscanrev(Width);
	bcy_=bitscanrev(Height);
	const u32 bcx=bcx_-3;
	const u32 bcy=bcy_-3;

	for (u32 y=0;y<Height;y+=PixelConvertor::ypp)
	{
		for (u32 x=0;x<Width;x+=PixelConvertor::xpp)
		{
			u8* p = &p_in[(twop(x,y,bcx,bcy)/divider)<<3];
			PixelConvertor::Convert(pb,p);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

template<class PixelConvertor, class pixel_type>
void texture_VQ(PixelBuffer<pixel_type>* pb,u8* p_in,u32 Width,u32 Height)
{
	p_in+=256*4*2;
	pb->amove(0,0);

	const u32 divider=PixelConvertor::xpp*PixelConvertor::ypp;
	u32 bcx_,bcy_;
	bcx_=bitscanrev(Width);
	bcy_=bitscanrev(Height);
	const u32 bcx=bcx_-3;
	const u32 bcy=bcy_-3;

	for (u32 y=0;y<Height;y+=PixelConvertor::ypp)
	{
		for (u32 x=0;x<Width;x+=PixelConvertor::xpp)
		{
			u8 p = p_in[twop(x,y,bcx,bcy)/divider];
			PixelConvertor::Convert(pb,&vq_codebook[p*8]);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

//Planar
#define tex565_PL32 texture_PL<conv565_PL32, u32>
#define tex1555_PL32 texture_PL<conv1555_PL32, u32>
#define tex4444_PL32 texture_PL<conv4444_PL32, u32>
#define texYUV422_PL32 texture_PL<convYUV_PL, u32>
#define texPAL4_PL32 texture_TW<convPAL4_PL, u32>
#define texPAL8_PL32  texture_TW<convPAL8_PL, u32>

//Twiddle
#define tex565_TW32 texture_TW<conv565_TW32, u32>
#define tex1555_TW32 texture_TW<conv1555_TW32, u32>
#define tex4444_TW32 texture_TW<conv4444_TW32, u32>
#define texYUV422_TW32 texture_TW<convYUV_TW, u32>
#define texPAL4_TW32 texture_TW<convPAL4_TW, u32>
#define texPAL8_TW32  texture_TW<convPAL8_TW, u32>


//VQ
#define tex565_VQ32 texture_VQ<conv565_TW32, u32>
#define tex1555_VQ32 texture_VQ<conv1555_TW32, u32>
#define tex4444_VQ32 texture_VQ<conv4444_TW32, u32>
#define texYUV422_VQ32 texture_VQ<convYUV_TW, u32>
