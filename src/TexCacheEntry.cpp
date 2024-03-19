/*
	This file is part of libswirl
*/
//#include "license/bsd"


#include <algorithm>
#include "TexCache.h"
#include "pvr_regs.h"
#include "pvr_mem.h"
#include "core_structs.h"

#include "TexConv.h"

/*
Textures

Textures are converted to native OpenGL textures
The mapping is done with tcw:tsp -> GL texture. That includes stuff like
filtering/ texture repeat

To save space native formats are used for 1555/565/4444 (only bit shuffling is done)
YUV is converted to 8888
PALs are decoded to their unpaletted format (5551/565/4444/8888 depending on palette type)

Mipmaps
	not supported for now

Compression
	look into it, but afaik PVRC is not realtime doable
*/

#if defined(REFSW_OFFLINE)
#define GLuint u32
#define GL_UNSIGNED_SHORT_5_6_5 0
#define GL_UNSIGNED_SHORT_5_5_5_1 1
#define GL_UNSIGNED_SHORT_4_4_4_4 2
#define GL_UNSIGNED_BYTE 3
#endif

u32 decoded_colors[3][65536];


struct PvrTexInfo;
template <class pixel_type> class PixelBuffer;
typedef void TexConvFP(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
typedef void TexConvFP32(PixelBuffer<u32>* pb,u8* p_in,u32 Width,u32 Height);

struct TextureCacheData
{
	TSP tsp;        //dreamcast texture parameters
	TCW tcw;
	
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

struct PvrTexInfo
{
	const char* name;
	int bpp;        //4/8 for pal. 16 for yuv, rgb, argb
	// Conversion to 32 bpp
	TexConvFP32 *PL32;
	TexConvFP32 *TW32;
	TexConvFP32 *VQ32;
};

PvrTexInfo format[8]=
{	// name     bpp Planar(32b)                 Twiddled(32b)                VQ (32b)
	{"1555",    16, (TexConvFP32*)tex1555_PL32, (TexConvFP32*)tex1555_TW32, (TexConvFP32*)tex1555_VQ32},  //1555
	{"565",     16, (TexConvFP32*)tex565_PL32,  (TexConvFP32*)tex565_TW32,  (TexConvFP32*)tex565_VQ32},   //565
	{"4444",    16, (TexConvFP32*)tex4444_PL32, (TexConvFP32*)tex4444_TW32, (TexConvFP32*)tex4444_VQ32},  //4444
	{"yuv",     16, (TexConvFP32*)texYUV422_PL32, (TexConvFP32*)texYUV422_TW32, (TexConvFP32*)texYUV422_VQ32},  //yuv
	{"bumpmap", 16, (TexConvFP32*)NULL},                                                                  //bump map
	{"pal4",    4,  (TexConvFP32*)NULL,         (TexConvFP32*)texPAL4_TW32, (TexConvFP32*)NULL},          //pal4
	{"pal8",    8,  (TexConvFP32*)NULL,         (TexConvFP32*)texPAL8_TW32,  (TexConvFP32*)NULL},         //pal8
	{"ns/1555", 0},                                                                                                                                                                                                  // Not supported (1555)
};

const u32 MipPoint[8] =
{
	0x00006,//8
	0x00016,//16
	0x00056,//32
	0x00156,//64
	0x00556,//128
	0x01556,//256
	0x05556,//512
	0x15556//1024
};

//Texture Cache :)
void TextureCacheData::PrintTextureName()
{
	printf("Texture: %s ",tex?tex->name:"?format?");

	if (tcw.VQ_Comp)
		printf(" VQ");

	if (tcw.ScanOrder==0)
		printf(" TW");

	if (tcw.MipMapped)
		printf(" MM");

	if (tcw.StrideSel)
		printf(" Stride");

	printf(" %dx%d @ 0x%X",8<<tsp.TexU,8<<tsp.TexV,tcw.TexAddr<<3);

	printf(" refsw-offline\n");
}

//Create GL texture from tsp/tcw
void TextureCacheData::Create()
{
	tex_type = 0;

	//Reset state info ..
	Lookups=0;

	//decode info from tsp/tcw into the texture struct
	tex=&format[tcw.PixelFmt == PixelReserved ? Pixel1555 : tcw.PixelFmt];	//texture format table entry
	
	sa_tex = (tcw.TexAddr<<3) & VRAM_MASK;	//texture start address
	sa = sa_tex;							//data texture start address (modified for MIPs, as needed)
	w=8<<tsp.TexU;                   //tex width
	h=8<<tsp.TexV;                   //tex height

	//PAL texture
	if (tex->bpp==4)
		indirect_color_ptr=tcw.PalSelect<<4;
	else if (tex->bpp==8)
		indirect_color_ptr=(tcw.PalSelect>>4)<<8;

	//VQ table (if VQ tex)
	if (tcw.VQ_Comp)
		indirect_color_ptr=sa;

	//Convert a pvr texture into OpenGL
	switch (tcw.PixelFmt)
	{

	case Pixel1555: 	//0     1555 value: 1 bit; RGB values: 5 bits each
	case PixelReserved: //7     Reserved        Regarded as 1555
	case Pixel565: 		//1     565      R value: 5 bits; G value: 6 bits; B value: 5 bits
	case Pixel4444: 	//2     4444 value: 4 bits; RGB values: 4 bits each
	case PixelYUV:		//3     YUV422 32 bits per 2 pixels; YUYV values: 8 bits each
	case PixelBumpMap:	//4		Bump Map 	16 bits/pixel; S value: 8 bits; R value: 8 bits
	case PixelPal4:		//5     4 BPP Palette   Palette texture with 4 bits/pixel
	case PixelPal8:		//6     8 BPP Palette   Palette texture with 8 bits/pixel
		if (tcw.ScanOrder)
		{
			verify(tex->PL32 != NULL);
			//Texture is stored 'planar' in memory, no deswizzle is needed
			//verify(tcw.VQ_Comp==0);
			if (tcw.VQ_Comp != 0)
				printf("Warning: planar texture with VQ set (invalid)\n");

			//Planar textures support stride selection, mostly used for non power of 2 textures (videos)
			int stride=w;
			if (tcw.StrideSel)
				stride=(TEXT_CONTROL&31)*32;
			//Call the format specific conversion code
			texconv32 = tex->PL32;
			//calculate the size, in bytes, for the locking
			size=stride*h*tex->bpp/8;
		}
		else
		{
			// Quake 3 Arena uses one. Not sure if valid but no need to crash
			//verify(w==h || !tcw.MipMapped); // are non square mipmaps supported ? i can't recall right now *WARN*

			if (tcw.VQ_Comp)
			{
				verify(tex->VQ32 != NULL);
				indirect_color_ptr=sa;
				if (tcw.MipMapped)
					sa+=MipPoint[tsp.TexU];
				texconv32 = tex->VQ32;
				size=w*h/8;
			}
			else
			{
				verify(tex->TW32 != NULL);
				if (tcw.MipMapped)
					sa+=MipPoint[tsp.TexU]*tex->bpp/2;
				texconv32 = tex->TW32;
				size=w*h*tex->bpp/8;
			}
		}
		break;
	default:
		printf("Unhandled texture %d\n",tcw.PixelFmt);
		size=w*h*2;
		texconv = NULL;
		texconv32 = NULL;
	}

	pData = (u16*)malloc(w * h * 4);

	if (size == 0) {
		size = 4;
	}
}


void TextureCacheData::CacheFromVram()
{
	//texture state tracking stuff

	palette_index=indirect_color_ptr; //might be used if pal. tex
	vq_codebook=(u8*)&vram[indirect_color_ptr];  //might be used if VQ tex

	//texture conversion work
	u32 stride=w;

	if (tcw.StrideSel && tcw.ScanOrder)
		stride=(TEXT_CONTROL&31)*32; //I think this needs +1 ?

	//PrintTextureName();
	u32 original_h = h;
	if (sa_tex > VRAM_SIZE || size == 0 || sa + size > VRAM_SIZE)
	{
		if (sa + size > VRAM_SIZE)
		{
			// Shenmue Space Harrier mini-arcade loads a texture that goes beyond the end of VRAM
			// but only uses the top portion of it
			h = (VRAM_SIZE - sa) * 8 / stride / tex->bpp;
			size = stride * h * tex->bpp/8;
		}
		else
		{
			printf("Warning: invalid texture. Address %08X %08X size %d\n", sa_tex, sa, size);
			return;
		}
	}

	void *temp_tex_buffer = NULL;

	PixelBuffer<u32> pb32;

	verify(texconv32 != NULL);

	pb32.init(w, h);

	texconv32(&pb32, (u8*)&vram[sa], stride, h);

	temp_tex_buffer = pb32.data();
	
	// Restore the original texture height if it was constrained to VRAM limits above
	h = original_h;

	//lock the texture to detect changes in it

	u32 *tex_data = (u32 *)temp_tex_buffer;

	memcpy(pData, tex_data, w * h * 4);

	if (tcw.VQ_Comp)
	{
		char temp[512];
		sprintf(temp, "dumps/vq_%x_%d_%d.table", indirect_color_ptr, w, h);
		{
			auto vq = fopen(temp, "wb");
			if (vq) {
				fwrite(vq_codebook, 1, 2048, vq);
				fclose(vq);
			}
			
		}

		{
			sprintf(temp, "dumps/vq_%x_%d_%d.index", indirect_color_ptr, w, h);
			auto vi = fopen(temp, "wb");
			if (vi) {
				fwrite(vram + sa_tex, 1, size, vi);
				fclose(vi);
			}
		}
	}
}

	
bool TextureCacheData::Delete()
{
	free(pData);
	pData = 0;
	
	return true;
}


#include <map>
map<u64,TextureCacheData> TexCache;
typedef map<u64,TextureCacheData>::iterator TexCacheIter;

TextureCacheData *getTextureCacheData(TSP tsp, TCW tcw);


//static float LastTexCacheStats;

// Only use TexU and TexV from TSP in the cache key
//     TexV : 7, TexU : 7
const TSP TSPTextureCacheMask = { { 7, 7 } };
//     TexAddr : 0x1FFFFF, Reserved : 0, StrideSel : 0, ScanOrder : 1, PixelFmt : 7, VQ_Comp : 1, MipMapped : 1
const TCW TCWTextureCacheMask = { { 0x1FFFFF, 0, 0, 1, 7, 1, 1 } };

TextureCacheData *getTextureCacheData(TSP tsp, TCW tcw) {
	u64 key = tsp.full & TSPTextureCacheMask.full;
	if (tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8)
		// Paletted textures have a palette selection that must be part of the key
		// We also add the palette type to the key to avoid thrashing the cache
		// when the palette type is changed. If the palette type is changed back in the future,
		// this texture will stil be available.
		key |= ((u64)tcw.full << 32) | ((PAL_RAM_CTRL & 3) << 6);
	else
		key |= (u64)(tcw.full & TCWTextureCacheMask.full) << 32;

	TexCacheIter tx = TexCache.find(key);

	TextureCacheData* tf;
	if (tx != TexCache.end())
	{
		tf = &tx->second;
		// Needed if the texture is updated
		tf->tcw.StrideSel = tcw.StrideSel;
	}
	else
	{
		tf=&TexCache[key];

		tf->tsp = tsp;
		tf->tcw = tcw;
	}

	return tf;
}

text_info raw_GetTexture(TSP tsp, TCW tcw)
{
	text_info rv = { 0 };

	TextureCacheData* tf = getTextureCacheData(tsp, tcw);

	if (!tf)
		return rv;

	if (tf->pData == 0) {
		tf->Create();
		tf->CacheFromVram();
	}


	//update state for opts/stuff
	tf->Lookups++;

	//return raw texture
	rv.height = tf->h;
	rv.width = tf->w;
	rv.pdata = tf->pData;
	rv.textype = tf->tex_type;
	
	return rv;
}