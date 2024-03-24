/*
	This file is part of libswirl

   Implementes the Reference SoftWare renderer (RefRendInterface) backend for refrend_base.
   This includes buffer operations and rasterization

   Pixel operations are in refsw_pixel.cpp (PixelPipeline interface)
*/
#include "license/bsd"

#include <cmath>
#include <memory>
#include <float.h>

#include "build.h"
#include "refsw_tile.h"
#include "TexUtils.h"
#include "TexCache.h"

parameter_tag_t tagBuffer    [MAX_RENDER_PIXELS];
StencilType     stencilBuffer[MAX_RENDER_PIXELS];
u32             colorBuffer1 [MAX_RENDER_PIXELS];
u32             colorBuffer2 [MAX_RENDER_PIXELS];
ZType           depthBuffer1 [MAX_RENDER_PIXELS];
ZType           depthBuffer2 [MAX_RENDER_PIXELS];

static float mmin(float a, float b, float c, float d)
{
    float rv = min(a, b);
    rv = min(c, rv);
    return max(d, rv);
}

static float mmax(float a, float b, float c, float d)
{
    float rv = max(a, b);
    rv = max(c, rv);
    return min(d, rv);
}

void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue)
{
    auto zb = depthBuffer1;
    auto stencil = stencilBuffer;
    auto pb = tagBuffer;

    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        zb[i] = depthValue;
        stencil[i] = stencilValue;
        pb[i] = paramValue;
    }
}

void ClearParamBuffer(parameter_tag_t paramValue) {
    auto pb = tagBuffer;

    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        pb[i] = paramValue;
    }
}

void PeelBuffers(float depthValue, u32 stencilValue)
{
    auto zb = depthBuffer1;
    auto zb2 = depthBuffer2;
    auto stencil = stencilBuffer;

    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        zb2[i] = zb[i];     // keep old ZB for reference
        zb[i] = depthValue;    // set the "closest" test to furthest value possible
        stencil[i] = stencilValue;
    }
}

void SummarizeStencilOr() {
    auto stencil = stencilBuffer;

    // post movdol merge INSIDE
    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        stencil[i] |= (stencil[i] >>1);
    }
}

void SummarizeStencilAnd() {
    auto stencil = stencilBuffer;

    // post movdol merge OUTSIDE
    for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
        stencil[i] &= (stencil[i] >>1);
    }
}

int PixelsDrawn;
void ClearPixelsDrawn()
{
    PixelsDrawn = 0;
}

u32 GetPixelsDrawn()
{
    return PixelsDrawn;
}

    // Render to ACCUM from TAG buffer
// TAG holds references to triangles, ACCUM is the tile framebuffer
void RenderParamTags(RenderMode rm, taRECT& rect) {
    float halfpixel = HALF_OFFSET.tsp_pixel_half_offset ? 0.5f : 0;

    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            auto index = y * 32 + x;
            auto tag =  tagBuffer[index];
            if (!(tag & TAG_INVALID)) {
                ISP_BACKGND_T_type t;
                t.full = tag;
                auto Entry = GetFpuEntry(&rect, rm, t);
                PixelFlush_tsp(rm == RM_PUNCHTHROUGH, &Entry, x + halfpixel, y + halfpixel, index, depthBuffer1[index]);
            }
        }
    }
}

void ClearFpuEntries() {

}

f32 f16(u16 v)
{
    u32 z=v<<16;
    return *(f32*)&z;
}

#define vert_packed_color_(to,src) \
	{ \
	u32 t=src; \
	to[0] = (u8)(t);t>>=8;\
	to[1] = (u8)(t);t>>=8;\
	to[2] = (u8)(t);t>>=8;\
	to[3] = (u8)(t);      \
	}

//decode a vertex in the native pvr format
void decode_pvr_vertex(DrawParameters* params, pvr32addr_t ptr, Vertex* cv, u32 shadow)
{
    //XYZ
    //UV
    //Base Col
    //Offset Col

    //XYZ are _allways_ there :)
    cv->x=vrf(ptr);ptr+=4;
    cv->y=vrf(ptr);ptr+=4;
    cv->z=vrf(ptr);ptr+=4;

    if (params->isp.Texture)
    {	//Do texture , if any
        if (params->isp.UV_16b)
        {
            u32 uv=vri(ptr);
            cv->u = f16((u16)(uv >>16));
            cv->v = f16((u16)(uv >> 0));
            ptr+=4;
        }
        else
        {
            cv->u=vrf(ptr);ptr+=4;
            cv->v=vrf(ptr);ptr+=4;
        }
    }

    //Color
    u32 col=vri(ptr);ptr+=4;
    vert_packed_color_(cv->col,col);
    if (params->isp.Offset)
    {
        //Intensity color
        u32 col=vri(ptr);ptr+=4;
        vert_packed_color_(cv->spc,col);
    }

    if (shadow) {
        if (params->isp.Texture)
        {	//Do texture , if any
            if (params->isp.UV_16b)
            {
                u32 uv=vri(ptr);
                cv->u1 = f16((u16)(uv >>16));
                cv->v1 = f16((u16)(uv >> 0));
                ptr+=4;
            }
            else
            {
                cv->u1=vrf(ptr);ptr+=4;
                cv->v1=vrf(ptr);ptr+=4;
            }
        }

        //Color
        u32 col1=vri(ptr);ptr+=4;
        vert_packed_color_(cv->col1,col1);
        if (params->isp.Offset)
        {
            //Intensity color
            u32 col1=vri(ptr);ptr+=4;
            vert_packed_color_(cv->spc1,col1);
        }
    }
}

// decode an object (params + vertexes)
u32 decode_pvr_vetrices(DrawParameters* params, pvr32addr_t base, u32 skip, u32 shadow, Vertex* vtx, int count, int offset)
{
    bool PSVM=FPU_SHAD_SCALE.intensity_shadow == 0;

    if (!PSVM) {
        shadow = 0; // no double volume stuff
    }

    params->isp.full=vri(base);
    params->tsp.full=vri(base+4);
    params->tcw.full=vri(base+8);

    base += 12;
    if (shadow) {
        params->tsp2.full=vri(base+0);
        params->tcw2.full=vri(base+4);
        base += 8;
    }

    for (int i = 0; i < offset; i++) {
        base += (3 + skip * (shadow+1)) * 4;
    }

    for (int i = 0; i < count; i++) {
        decode_pvr_vertex(params,base, &vtx[i], shadow);
        base += (3 + skip * (shadow+1)) * 4;
    }

    return base;
}

FpuEntry GetFpuEntry(taRECT *rect, RenderMode render_mode, ISP_BACKGND_T_type core_tag)
{
    FpuEntry entry = {};
    Vertex vtx[3];
    decode_pvr_vetrices(&entry.params, PARAM_BASE + core_tag.tag_address_in_words * 4, core_tag.skip, core_tag.shadow, vtx, 3, core_tag.tag_offset);
    // generate
    if (entry.params.isp.Texture)
    {
        entry.texture = {
            .width = 8U << entry.params.tsp.TexU,
            .height = 8U << entry.params.tsp.TexV,
            .tsp = entry.params.tsp,
            .tcw = entry.params.tcw,
        };
    }

    entry.ips.Setup(rect, &entry.params, &entry.texture, vtx[0], vtx[1], vtx[2]);

    return entry;
}

// Lookup/create cached TSP parameters, and call PixelFlush_tsp
bool PixelFlush_tsp(bool pp_AlphaTest, FpuEntry* entry, float x, float y, u32 index, float invW)
{   
    return PixelFlush_tsp(entry->params.tsp.UseAlpha, entry->params.isp.Texture, entry->params.isp.Offset, entry->params.tsp.ColorClamp, entry->params.tsp.FogCtrl,
                                        entry->params.tsp.IgnoreTexA, entry->params.tsp.ClampU, entry->params.tsp.ClampV, entry->params.tsp.FlipU,  entry->params.tsp.FlipV,  entry->params.tsp.FilterMode,  entry->params.tsp.ShadInstr,  
                                        pp_AlphaTest,  entry->params.tsp.SrcSelect,  entry->params.tsp.DstSelect,  entry->params.tsp.SrcInstr,  entry->params.tsp.DstInstr,
                                        entry, x, y, 1/invW, index);
}

// Rasterize a single triangle to ISP (or ISP+TSP for PT)
void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area)
{
    const int stride_bytes = STRIDE_PIXEL_OFFSET * 4;
    //Plane equation

#define FLUSH_NAN(a) isnan(a) ? 0 : a

    float Y1 = FLUSH_NAN(v1.y);
    float Y2 = FLUSH_NAN(v2.y);
    float Y3 = FLUSH_NAN(v3.y);
    float Y4 = v4 ? FLUSH_NAN(v4->y) : 0;

    float X1 = FLUSH_NAN(v1.x);
    float X2 = FLUSH_NAN(v2.x);
    float X3 = FLUSH_NAN(v3.x);
    float X4 = v4 ? FLUSH_NAN(v4->x) : 0;

    int sgn = 1;

    float tri_area = ((X1 - X3) * (Y2 - Y3) - (Y1 - Y3) * (X2 - X3));

    if (tri_area > 0)
        sgn = -1;

    // cull
    if (params->isp.CullMode != 0) {
        //area: (X1-X3)*(Y2-Y3)-(Y1-Y3)*(X2-X3)

        float abs_area = fabsf(tri_area);

        if (abs_area < FPU_CULL_VAL)
            return;

        if (params->isp.CullMode >= 2) {
            u32 mode = params->isp.CullMode & 1;

            if (
                (mode == 0 && tri_area < 0) ||
                (mode == 1 && tri_area > 0)) {
                return;
            }
        }
    }

	if (sgn < 0) {
		float tmp;
		#define SWAP(A,B)	{ tmp=A; A=B; B=tmp; }
		SWAP(X2,X3);
		SWAP(Y2,Y3);
		#undef SWAP
	}


	// ============================================================================
	//   Screen space stuff done at setup time.
	// ============================================================================

    // Half-edge constants
    const float DX12 =      (X1 - X2);
    const float DX23 =      (X2 - X3);
    const float DX31 = v4 ? (X3 - X4) : (X3 - X1);
    const float DX41 = v4 ? (X4 - X1) : 0;

    const float DY21 =      (Y2 - Y1);
    const float DY32 =      (Y3 - Y2);
    const float DY13 = v4 ? (Y4 - Y3) : (Y1 - Y3);
    const float DY14 = v4 ? (Y1 - Y4) : 0;

    float C1 =      (-DY21 * X1) - (DX12 * Y1);
    float C2 =      (-DY32 * X2) - (DX23 * Y2);
    float C3 =      (-DY13 * X3) - (DX31 * Y3);
    float C4 = v4 ? (-DY14 * X4) - (DX41 * Y4) : 1;

    PlaneStepper3Tile Z;
    Z.Setup(v1, v2, v3, v1.z, v2.z, v3.z);

    float halfpixel = HALF_OFFSET.fpu_pixel_half_offset ? 0.5f : 0;
    float y_ps    = halfpixel;
    float minx_ps = halfpixel;

	// ============================================================================
	//   Tile space stuff done when selecting the tile.
	// ============================================================================

	/*
		s64 orient2dC(const Vec3C& a, const Vec3C& b, const Point2D& c)
		{
			return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
		}

		bool isTopLeft(Vec3C& start, Vec3C& end) {
			int dx = end.x - start.x;
			int dy = end.y - start.y;

			bool isTop  = (dy == 0) && (dx > 0);
			bool isLeft =  dy < 0;
			return isTop || isLeft;
		}
	
		int bias0    = 
		int bias1    = 
		int bias2    = 

		s64 w0_start = orient2dC(v2, v3, p) + bias0; // DX23*(Y2) + DY32*(X2); // C2    => isTopLeft(v2, v3) ? 0 : -1;
		s64 w2_start = orient2dC(v1, v2, p) + bias2; // DX12*(Y1) + DY21*(X1); // C1    => isTopLeft(v1, v2) ? 0 : -1;
		s64 w1_start = orient2dC(v3, v1, p) + bias1; // DX31*(Y3) + DY31*(X3); // C3    => isTopLeft(v3, v1) ? 0 : -1;
	*/

	C1 +=         DY21 * (area->tileX*32) + DX12 * ( area->tileY*32);
	C2 +=         DY32 * (area->tileX*32) + DX23 * ( area->tileY*32);
	C3 +=         DY13 * (area->tileX*32) + DX31 * ( area->tileY*32);
	C4 += v4 ?  (-DY14)* (area->tileX*32) - DX41 * ( area->tileY*32) : 0;

	Z.SetTile(area->tileX,area->tileY);

	// ============================================================================
	//   Inside Tile space.
	// ============================================================================

    // Loop through ALL pixels in the tile (no smart clipping)
	for (int y = 0; y < 32; y++)
    {
        float x_ps = minx_ps;
        for (int x = 0; x < 32; x++)
        {
            float Xhs12 = C1 + DX12 * y_ps + DY21 * x_ps;
            float Xhs23 = C2 + DX23 * y_ps + DY32 * x_ps;
            float Xhs31 = C3 + DX31 * y_ps + DY13 * x_ps;
            float Xhs41 = C4 + DX41 * y_ps + DY14 * x_ps;

            bool inTriangle = Xhs12 >= 0 && Xhs23 >= 0 && Xhs31 >= 0 && Xhs41 >= 0;
			
            if (inTriangle) {
                u32 index = y * 32 + x;
                float invW = Z.Ip(x_ps, y_ps);
                PixelFlush_isp(render_mode, params->isp.DepthMode, x_ps, y_ps, invW, index, tag);
            }

            x_ps = x_ps + 1;
        }
    next_y:
        y_ps = y_ps + 1;
    }
}
    
u8* GetColorOutputBuffer() {
    return (u8*)colorBuffer1;
}


// Clamp and flip a texture coordinate
static int ClampFlip( bool pp_Clamp, bool pp_Flip
	                , int coord, int size) {
    if (pp_Clamp) { // clamp
        if (coord < 0) {
            coord = 0;
        } else if (coord >= size) {
            coord = size-1;
        }
    } else if (pp_Flip) { // flip
        coord &= size*2-1;
        if (coord & size) {
            coord ^= size*2-1;
        }
    } else { //wrap
        coord &= size-1;
    }

    return coord;
}


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

#if 0
static Color TextureFetchOld(const text_info *texture, int u, int v) {
    auto textel_stride = 8 << texture->tsp.TexU;

    u32 start_address = texture->tcw.TexAddr << 3;
    u32 base_address = start_address;
    
    u32 mip_bpp;
    if (texture->tcw.VQ_Comp) {
        mip_bpp = 2;
    } else if (texture->tcw.PixelFmt == PixelPal8) {
        mip_bpp = 8;
    }
    else if (texture->tcw.PixelFmt == PixelPal4) {
        mip_bpp = 4;
    }
    else {
        mip_bpp = 16;
    }

    if (texture->tcw.MipMapped) {
        base_address += MipPoint[texture->tsp.TexU] * mip_bpp / 2;
    }

    u32 offset;
    if (texture->tcw.VQ_Comp) {
        offset = twop(u, v, texture->tsp.TexU, texture->tsp.TexV) / 4;
    } else if (!texture->tcw.ScanOrder) {
        offset = twop(u, v, texture->tsp.TexU, texture->tsp.TexV);
    } else {
        offset = u + textel_stride * v;
    }

    u16 memtel;
    if (texture->tcw.VQ_Comp) {
        u8 index = vram[(base_address + offset + 256*4*2) & VRAM_MASK];
        u16 *vq_book = (u16*)&vram[start_address];
        memtel = vq_book[index * 4 + (u&1)*2 + (v&1) ];
    } else {
        memtel = (u16&)vram[(base_address + offset *2) & VRAM_MASK];
    }

    u32 textel;
    switch (texture->tcw.PixelFmt)
    {
        case PixelReserved:
        case Pixel1555: textel = ARGB1555_32(memtel); break;
        case Pixel565: textel = ARGB565_32(memtel); break;
        case Pixel4444: textel = ARGB4444_32(memtel); break;
        case PixelYUV: textel = 0xDE000EEF; break;
        case PixelBumpMap: textel = 0xDEADBEEF; break;
        case PixelPal4: textel = 0xFF00FFF0; break;
        case PixelPal8: textel = 0xFF0FF0FF; break;
    }
    return { .raw =  textel };
}
#endif

u32 ExpandToARGB8888(u32 color, u32 mode, bool ScanOrder /* TODO: Expansion Patterns */) {
	switch(mode)
	{
        case 0: return ARGB1555_32(color);
        case 1: return ARGB565_32(color);
        case 2: return ARGB4444_32(color);
        case 3: return ARGB8888_32(color);  // this one just shuffles
    }
    return 0xDEADBEEF;
}

u32 TexAddressGen(const text_info *texture) {
    u32 base_address = texture->tcw.TexAddr << 3;

    if (texture->tcw.VQ_Comp) {
        base_address += 256 * 4 * 2;
    }

    return base_address;
}

u32 TexOffsetGen(const text_info *texture, int u, int v, u32 stride, u32 MipLevel) {
    u32 mip_offset;
    
    if (texture->tcw.MipMapped) {
        mip_offset = MipPoint[texture->tsp.TexU - MipLevel] * 4;
    } else {
        mip_offset = 0;
    }

    if (texture->tcw.VQ_Comp || !texture->tcw.ScanOrder) {
        return mip_offset + twop(u, v, (texture->tsp.TexU - MipLevel), (texture->tsp.TexV - MipLevel));
    } else {
        return mip_offset + u + stride * v;
    }
}

u32 BitsPerPixel(const text_info *texture) {
    if (texture->tcw.VQ_Comp) {
        return 2; // 8 bpp / 2 / 2 in each direction
    } else if (texture->tcw.PixelFmt == PixelPal8) {
        return 8;
    }
    else if (texture->tcw.PixelFmt == PixelPal4) {
        return 4;
    }
    else {
        return 16;
    }
}

u64 VQLookup(u32 start_address, u64 memtel, u32 offset) {
    u8* memtel8 = (u8*)&memtel;

    u64 *vq_book = (u64*)&vram[start_address & (VRAM_MASK-7)];
    u8 index = memtel8[offset & 7];
    return vq_book[index];
}

u32 TexStride(const text_info *texture, u32 MipLevel) {
    if (texture->tcw.StrideSel && texture->tcw.ScanOrder)
		return (TEXT_CONTROL&31)*32;
    else
        return (8U << texture->tsp.TexU) >> MipLevel;
}

u32 DecodeTextel(u32 PixelFmt, u32 PalSelect, u64 memtel, u32 offset) {
    auto memtel_32 = (u32*)&memtel;
    auto memtel_16 = (u16*)&memtel;
    auto memtel_8 = (u8*)&memtel;

    switch (PixelFmt)
    {
        case PixelReserved:
        case Pixel1555:
        case Pixel565:
        case Pixel4444:
        case PixelBumpMap:
            return memtel_16[offset & 3]; break;

        case PixelYUV: {
            auto memtel_yuv = memtel_32[(offset >> 1) & 1];
            auto memtel_yuv8 = (u8*)&memtel_yuv;
            return offset & 1
                ? YUV422(memtel_yuv8[0], memtel_yuv8[1], memtel_yuv8[2])
                : YUV422(memtel_yuv8[3], memtel_yuv8[1], memtel_yuv8[2]);
            }

        case PixelPal4: {
            auto local_idx = (memtel >> (offset & 15)*4) & 15;
            auto idx = PalSelect * 16 | local_idx;
            return PALETTE_RAM[idx];
        }
        break;
        case PixelPal8: {
            auto local_idx = memtel_8[offset & 7];
            auto idx = (PalSelect / 16) * 256 | local_idx;
            return PALETTE_RAM[idx];
        }
        break;
    }
    return 0xDEADBEEF;
}

u32 GetExpandFormat(u32 PixelFmt) {
    if (PixelFmt == PixelPal4 || PixelFmt == PixelPal8) {
        return PAL_RAM_CTRL&3;
    } else if (PixelFmt == PixelBumpMap || PixelFmt == PixelYUV) {
        return 3;
    } else {
        return PixelFmt & 3;
    }
}

static Color TextureFetch(const text_info *texture, int u, int v, u32 MipLevel) {
    
    if (MipLevel > texture->tsp.TexU)
        MipLevel = texture->tsp.TexU;
    if (MipLevel > texture->tsp.TexV)
        MipLevel = texture->tsp.TexV;
    
    u >>= MipLevel;
    v >>= MipLevel;

    u32 stride = TexStride(texture, MipLevel);

    u32 start_address = texture->tcw.TexAddr << 3;

    auto bpp = BitsPerPixel(texture);
    
    auto base_address = TexAddressGen(texture);
    auto offset = TexOffsetGen(texture, u, v, stride, MipLevel);

    u64 memtel = (u64&)vram[(base_address + offset * bpp / 8) & (VRAM_MASK-7)];

    if (texture->tcw.VQ_Comp) {
        memtel = VQLookup(start_address, memtel, offset / 4);
    }

    u32 textel = DecodeTextel(texture->tcw.PixelFmt, texture->tcw.PalSelect, memtel, offset);

    u32 expand_format = GetExpandFormat(texture->tcw.PixelFmt);

    

    textel = ExpandToARGB8888(textel, expand_format, texture->tcw.ScanOrder);

    // auto old = TextureFetch2(texture, u, v);
    // if (textel != old.raw) {
    //     textel = TextureFetch2(texture, u, v).raw;
    //     textel = TextureFetch(texture, u, v).raw;
    //     die("Missmatch");
    // }
    // auto old = raw_GetTexture(texture->tsp, texture->tcw)[u + v * stride];
    // if (textel != old) {
    //     die("Missmatch");
    // }
    // This uses the old path for debugging
    // return { .raw = raw_GetTexture(texture->tsp, texture->tcw)[u + v * textel_stride] };
    return { .raw =  textel };
    // return {
    //     .b = (8-MipLevel) * 15,
    //     .g = (8-MipLevel) * 15,
    //     .r = (8-MipLevel) * 15,
    //     .a = 255,
    // };
}
// Fetch pixels from UVs, interpolate
static Color TextureFilter(
	bool pp_IgnoreTexA,  bool pp_ClampU, bool pp_ClampV, bool pp_FlipU, bool pp_FlipV, u32 pp_FilterMode,
	const text_info *texture, float u, float v, u32 MipLevel, f32 dTrilinear) {
        
    int halfpixel = HALF_OFFSET.texure_pixel_half_offset ? -127 : 0;
        
    int ui = u * 256 + halfpixel;
    int vi = v * 256 + halfpixel;

    auto offset00 = TextureFetch(texture, ClampFlip(pp_ClampU, pp_FlipU, (ui >> 8) + 1, texture->width), ClampFlip(pp_ClampV, pp_FlipV, (vi >> 8) + 1, texture->height), MipLevel);
    auto offset01 = TextureFetch(texture, ClampFlip(pp_ClampU, pp_FlipU, (ui >> 8) + 0, texture->width), ClampFlip(pp_ClampV, pp_FlipV, (vi >> 8) + 1, texture->height), MipLevel);
    auto offset10 = TextureFetch(texture, ClampFlip(pp_ClampU, pp_FlipU, (ui >> 8) + 1, texture->width), ClampFlip(pp_ClampV, pp_FlipV, (vi >> 8) + 0, texture->height), MipLevel);
    auto offset11 = TextureFetch(texture, ClampFlip(pp_ClampU, pp_FlipU, (ui >> 8) + 0, texture->width), ClampFlip(pp_ClampV, pp_FlipV, (vi >> 8) + 0, texture->height), MipLevel);

    Color textel = {0xAF674839};

    if (pp_FilterMode == 0) {
        // Point sampling
        for (int i = 0; i < 4; i++)
        {
            textel = offset11;
        }
    } else if (pp_FilterMode == 1) {
        // Bilinear filtering
        int ublend = ui & 255;
        int vblend = vi & 255;
        int nublend = 255 - ublend;
        int nvblend = 255 - vblend;

        for (int i = 0; i < 4; i++)
        {
            textel.bgra[i] =
                (offset00.bgra[i] * ublend * vblend) / 65536 +
                (offset01.bgra[i] * nublend * vblend) / 65536 +
                (offset10.bgra[i] * ublend * nvblend) / 65536 +
                (offset11.bgra[i] * nublend * nvblend) / 65536;
        };
    } else {
        // trilinear filtering A and B
        die("pp_FilterMode is trilinear");
    }
        

    if (pp_IgnoreTexA)
    {
        textel.a = 255;
    }

    return textel;
}

// Combine Base, Textel and Offset colors
static Color ColorCombiner(
	bool pp_Texture, bool pp_Offset, u32 pp_ShadInstr,
	Color base, Color textel, Color offset) {

    Color rv = base;
    if (pp_Texture)
    {
        if (pp_ShadInstr == 0)
        {
            //color.rgb = texcol.rgb;
            //color.a = texcol.a;

            rv = textel;
        }
        else if (pp_ShadInstr == 1)
        {
            //color.rgb *= texcol.rgb;
            //color.a = texcol.a;
            for (int i = 0; i < 3; i++)
            {
                rv.bgra[i] = textel.bgra[i] * base.bgra[i] / 256;
            }

            rv.a = textel.a;
        }
        else if (pp_ShadInstr == 2)
        {
            //color.rgb=mix(color.rgb,texcol.rgb,texcol.a);
            u8 tb = textel.a;
            u8 cb = 255 - tb;

            for (int i = 0; i < 3; i++)
            {
                rv.bgra[i] = (textel.bgra[i] * tb + base.bgra[i] * cb) / 256;
            }

            rv.a = base.a;
        }
        else if (pp_ShadInstr == 3)
        {
            //color*=texcol
            for (int i = 0; i < 4; i++)
            {
                rv.bgra[i] = textel.bgra[i] * base.bgra[i] / 256;
            }
        }

        if (pp_Offset) {
            // mix only color, saturate
            for (int i = 0; i < 3; i++)
            {
                rv.bgra[i] = min(rv.bgra[i] + offset.bgra[i], 255);
            }
        }
    }

    return rv;
}

static Color BumpMapper(Color textel, Color offset) {
    u8 K1 = offset.a;
    u8 K2 = offset.r;
    u8 K3 = offset.g;
    u8 Q = offset.b;

    u8 S = offset.a;
    u8 R = offset.r;
    
    u8 I = u8(K1 + K2*BM_SIN90[S]/256 + K3*BM_COS90[S]*BM_COS360[(R - Q) & 255]/256/256);

	Color res;
	res.b = 255;
	res.g = 255;
	res.r = 255;
	res.a = I;
    return res;
}

// Interpolate the base color, also cheap shadows modifier
static Color InterpolateBase(
	bool pp_UseAlpha, bool pp_CheapShadows,
	const PlaneStepper3* Col, float x, float y, float W, u32 stencil) {
    Color rv;
    u32 mult = 256;

    if (pp_CheapShadows) {
        if (stencil & 1) {
            mult = FPU_SHAD_SCALE.scale_factor;
        }
    }

    rv.bgra[0] = Col[0].Ip(x, y, W) * mult / 256;
    rv.bgra[1] = Col[1].Ip(x, y, W) * mult / 256;
    rv.bgra[2] = Col[2].Ip(x, y, W) * mult / 256;
    rv.bgra[3] = Col[3].Ip(x, y, W) * mult / 256;    

    if (!pp_UseAlpha)
    {
        rv.a = 255;
    }

    return rv;
}

// Interpolate the offset color, also cheap shadows modifier
static Color InterpolateOffs(bool pp_CheapShadows,
	const PlaneStepper3* Ofs, float x, float y, float W, u32 stencil) {
    Color rv;
    u32 mult = 256;

    if (pp_CheapShadows) {
        if (stencil & 1) {
            mult = FPU_SHAD_SCALE.scale_factor;
        }
    }

    rv.bgra[0] = Ofs[0].Ip(x, y, W) * mult / 256;
    rv.bgra[1] = Ofs[1].Ip(x, y, W) * mult / 256;
    rv.bgra[2] = Ofs[2].Ip(x, y, W) * mult / 256;
    rv.bgra[3] = Ofs[3].Ip(x, y, W);

    return rv;
}

// select/calculate blend coefficient for the blend unit
static Color BlendCoefs(
	u32 pp_AlphaInst, bool srcOther,
	Color src, Color dst) {
    Color rv;

    switch(pp_AlphaInst>>1) {
        // zero
        case 0: rv.raw = 0; break;
        // other color
        case 1: rv = srcOther? src : dst; break;
        // src alpha
        case 2: for (int i = 0; i < 4; i++) rv.bgra[i] = src.a; break;
        // dst alpha
        case 3: for (int i = 0; i < 4; i++) rv.bgra[i] = dst.a; break;
    }

    if (pp_AlphaInst & 1) {
        for (int i = 0; i < 4; i++)
            rv.bgra[i] = 255 - rv.bgra[i];
    }

    return rv;
}

// Blending Unit implementation. Alpha blend, accum buffers and such
static bool BlendingUnit(
	bool pp_AlphaTest, u32 pp_SrcSel, u32 pp_DstSel, u32 pp_SrcInst, u32 pp_DstInst,
	u32 index, Color col)
{
    Color rv;
    Color src = {.raw  = pp_SrcSel ? colorBuffer2[index] : col.raw };
    Color dst = {.raw = pp_DstSel ? colorBuffer2[index] : colorBuffer1[index] };
        
    Color src_blend = BlendCoefs(pp_SrcInst, false, src, dst);
    Color dst_blend = BlendCoefs(pp_DstInst, true, src, dst);

    for (int j = 0; j < 4; j++)
    {
        rv.bgra[j] = min((src.bgra[j] * src_blend.bgra[j] + dst.bgra[j] * dst_blend.bgra[j]) >> 8, 255);
    }

    if (!pp_AlphaTest || src.a >= PT_ALPHA_REF)
    {
        (pp_DstSel ? colorBuffer2[index] : colorBuffer1[index]) = rv.raw;
        return true;
    }
    else
    {
        return false;
    }
}

static u8 LookupFogTable(float invW) {
    u8* fog_density=(u8*)&FOG_DENSITY;
    float fog_den_mant=fog_density[1]/128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
    s32 fog_den_exp=(s8)fog_density[0];
        
    float fog_den = fog_den_mant*powf(2.0f,fog_den_exp);

    f32 fogW = fog_den * invW;

    fogW = max(fogW, 1.0f);
    fogW = min(fogW, 255.999985f);

    union f32_fields {
        f32 full;
        struct {
            u32 m: 23;
            u32 e: 8;
            u32 s: 1;
        };
    };

    f32_fields fog_fields = { fogW };

    u32 index = (((fog_fields.e +1) & 7) << 4) |  ((fog_fields.m>>19) & 15);

    u8 blendFactor = (fog_fields.m>>11) & 255;
    u8 blend_inv = 255^blendFactor;

    auto fog_entry = (u8*)&FOG_TABLE[index];

    u8 fog_alpha = (fog_entry[0] * blendFactor + fog_entry[1] * blend_inv) >> 8;

    return fog_alpha;
}

// Color Clamp and Fog a pixel
static Color FogUnit(bool pp_Offset, bool pp_ColorClamp, u32 pp_FogCtrl, Color col, float invW, u8 offs_a) {
    if (pp_ColorClamp) {
        Color clamp_max = { FOG_CLAMP_MAX };
        Color clamp_min = { FOG_CLAMP_MIN };

        for (int i = 0; i < 4; i++)
        {
            col.bgra[i] = min(col.bgra[i], clamp_max.bgra[i]);
            col.bgra[i] = max(col.bgra[i], clamp_min.bgra[i]);
        }
    }

    switch(pp_FogCtrl) {
        // Look up mode 1
        case 0b00:
        // look up mode 2
        case 0b11:
            {
                u8 fog_alpha = LookupFogTable(invW);
                    
                u8 fog_inv = 255^fog_alpha;

                Color col_ram = { FOG_COL_RAM };

                if (pp_FogCtrl == 0b00) {
                    for (int i = 0; i < 3; i++) {
                        col.bgra[i] = (col.bgra[i] * fog_inv + col_ram.bgra[i] * fog_alpha)>>8;
                    }
                } else {
                    for (int i = 0; i < 3; i++) {
                        col.bgra[i] = col_ram.bgra[i];
                    }
                    col.a = col_ram.a;
                }
            }
            break;

        // Per Vertex
        case 0b01:
            if (pp_Offset) {
                Color col_vert = { FOG_COL_VERT };
                u8 alpha = offs_a;
                u8 inv = 255^alpha;
                  
                for (int i = 0; i < 3; i++)
                {
                    col.bgra[i] = (col.bgra[i] * inv + col_vert.bgra[i] * alpha)>>8;
                }
            }
            break;

                
        // No Fog
        case 0b10:
            break;
    }

    return col;
}

// Implement the full texture/shade pipeline for a pixel
bool PixelFlush_tsp(
	bool pp_UseAlpha, bool pp_Texture, bool pp_Offset, bool pp_ColorClamp, u32 pp_FogCtrl, bool pp_IgnoreAlpha, bool pp_ClampU, bool pp_ClampV, bool pp_FlipU, bool pp_FlipV, u32 pp_FilterMode, u32 pp_ShadInstr, bool pp_AlphaTest, u32 pp_SrcSel, u32 pp_DstSel, u32 pp_SrcInst, u32 pp_DstInst,
	const FpuEntry *entry, float x, float y, float W, u32 index)
{
    auto stencil = stencilBuffer + index;
    auto cb = (Color*)colorBuffer1 + index;
    auto pb = tagBuffer + index;

    *pb |= TAG_INVALID;

    Color base = { 0 }, textel = { 0 }, offs = { 0 };

    base = InterpolateBase(pp_UseAlpha, true, entry->ips.Col, x, y, W, *stencil);
        
    if (pp_Texture) {
        float u = entry->ips.U.Ip(x, y, W);
        float v = entry->ips.V.Ip(x, y, W);

        float dTrilinear;
        u32 MipLevel;
        if (pp_FilterMode != 0 && entry->texture.tcw.MipMapped) {
            // faux mip map cals
            // these really don't follow hw
            float ddx = (entry->ips.U.ddx + entry->ips.V.ddx);
            float ddy = (entry->ips.V.ddy + entry->ips.V.ddy);

            float dMip = fmaxf(fabsf(ddx), fabsf(ddy)) * W;

            MipLevel = 0; // biggest
            while(dMip > 2 && MipLevel < 8) {
                MipLevel ++;
                dMip = dMip / 2;
            }
            dTrilinear = dMip;
        } else {
            dTrilinear = 0;
            MipLevel = 0;
        }
        
        textel = TextureFilter(pp_IgnoreAlpha, pp_ClampU, pp_ClampV, pp_FlipU, pp_FlipV, pp_FilterMode, &entry->texture, u, v, MipLevel, dTrilinear);
        if (pp_Offset) {
            offs = InterpolateOffs(true, entry->ips.Ofs, x, y, W, *stencil);
        }
    }

    Color col;
    if (pp_Texture && pp_Offset && entry->texture.tcw.PixelFmt == PixelBumpMap) {
        col = BumpMapper(textel, offs);
    } else {
        col = ColorCombiner(pp_Texture, pp_Offset, pp_ShadInstr, base, textel, offs);
    }
        
    col = FogUnit(pp_Offset, pp_ColorClamp, pp_FogCtrl, col, 1/W, offs.a);

	return BlendingUnit(pp_AlphaTest, pp_SrcSel, pp_DstSel, pp_SrcInst, pp_DstInst, index, col);
}

// Depth processing for a pixel -- render_mode 0: OPAQ, 1: PT, 2: TRANS
void PixelFlush_isp(RenderMode render_mode, u32 depth_mode, float x, float y, float invW, u32 index, parameter_tag_t tag)
{
    auto pb = tagBuffer + index;
    auto zb = depthBuffer1 + index;
    auto zb2 = depthBuffer2 + index;
    auto stencil = stencilBuffer + index;

    auto mode = depth_mode;
        
    if (render_mode == RM_PUNCHTHROUGH)
        mode = 6; // TODO: FIXME
    else if (render_mode == RM_TRANSLUCENT)
        mode = 3; // TODO: FIXME
    else if (render_mode == RM_MODIFIER)
        mode = 6;
        
    switch(mode) {
        // never
        case 0: return; break;
        // less
        case 1: if (invW >= *zb) return; break;
        // equal
        case 2: if (invW != *zb) return; break;
        // less or equal
        case 3: if (invW > *zb) return; break;
        // greater
        case 4: if (invW <= *zb) return; break;
        // not equal
        case 5: if (invW == *zb) return; break;
        // greater or equal
        case 6: if (invW < *zb) return; break;
        // always
        case 7: break;
    }

    switch (render_mode)
    {
        // OPAQ
        case RM_OPAQUE:
        {
            // Z pre-pass only

            *zb = invW;
            *pb = tag;
        }
        break;

        case RM_MODIFIER:
        {
            // Flip on Z pass

            *stencil ^= 0b10;
        }
        break;

        // PT
        case RM_PUNCHTHROUGH:
        {
            // Z + TSP syncronized for alpha test
            // TODO: Implement layer peeling front-to-back here
            //if (AlphaTest_tsp(x, y, pb, invW, tag))
            {
                *zb = invW;
                *pb = tag;
            }
        }
        break;

        // Layer Peeling. zb2 holds the reference depth, zb is used to find closest to reference
        case RM_TRANSLUCENT:
        {
            if (invW < *zb2)
                return;

            if (invW == *zb || invW == *zb2) {
                return; // TODO: FIGURE OUT WHY THIS IS BROKEN and/or needed
                auto tagExisting = *(parameter_tag_t *)pb;

                if (tagExisting & TAG_INVALID)
                {
                    if (tag < tagExisting)
                        return;
                }
                else
                {
                    if (tag >= tagExisting)
                        return;
                }
            }

            PixelsDrawn++;

            *zb = invW;
            *pb = tag;
        }
        break;
    }
}