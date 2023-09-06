/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include <unordered_map>
#include <atomic>



//vertex types
extern u32 gcflip;
extern float scale_x, scale_y;


void DrawStrips();

extern float fb_scale_x, fb_scale_y;


struct text_info {
	u16* pdata;
	u32 width;
	u32 height;
	u32 textype; // 0 565, 1 1555, 2 4444
};
enum ModifierVolumeMode { Xor, Or, Inclusion, Exclusion, ModeCount };


text_info raw_GetTexture(u8* vram, TSP tsp, TCW tcw);
void killtex();
void CollectCleanup();
void DoCleanup();
void SortPParams(int first, int count);
void SetCull(u32 CullMode);

void SetMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc);

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt);
void ReadRTTBuffer(u8* vram);
void RenderFramebuffer();
void DrawFramebuffer(float w, float h);

bool render_output_framebuffer();
void free_output_framebuffer();

struct PvrTexInfo;
template <class pixel_type> class PixelBuffer;
typedef void TexConvFP(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
typedef void TexConvFP32(PixelBuffer<u32>* pb,u8* p_in,u32 Width,u32 Height);

struct TextureCacheData
{
	TSP tsp;        //dreamcast texture parameters
	TCW tcw;
	u8* vram;
	
	u16* pData;
	int tex_type;
	
	u32 Lookups;
	
	//decoded texture info
	u32 sa;         //pixel data start address in vram (might be offset for mipmaps/etc)
	u32 sa_tex;		//texture data start address in vram
	u32 w,h;        //width & height of the texture
	u32 size;       //size, in bytes, in vram
	
	PvrTexInfo* tex;
	TexConvFP*  texconv;
	TexConvFP32*  texconv32;
	
	//used for palette updates
	u32 palette_hash;			// Palette hash at time of last update
	u32  indirect_color_ptr;    //palette color table index for pal. tex
	//VQ quantizers table for VQ tex
	//a texture can't be both VQ and PAL at the same time
	u32 texture_hash;			// xxhash of texture data, used for custom textures
	u32 old_texture_hash;		// legacy hash
	
	
	void PrintTextureName();
	
	bool IsPaletted()
	{
		return tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8;
	}
	
	void Create();

	void CacheFromVram();

	//true if : dirty or paletted texture and hashes don't match
	bool Delete();
};
