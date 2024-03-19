#pragma once
/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "pvr_regs.h"
#include "pvr_mem.h"
#include "core_structs.h"

#include "TexCache.h"

#include "refsw_lists.h"

// For texture cache

#define MAX_RENDER_WIDTH 32
#define MAX_RENDER_HEIGHT 32
#define MAX_RENDER_PIXELS (MAX_RENDER_WIDTH * MAX_RENDER_HEIGHT)

#define STRIDE_PIXEL_OFFSET MAX_RENDER_WIDTH

#define PARAM_BUFFER_PIXEL_OFFSET   0
#define DEPTH1_BUFFER_PIXEL_OFFSET  (MAX_RENDER_PIXELS*1)
#define DEPTH2_BUFFER_PIXEL_OFFSET  (MAX_RENDER_PIXELS*2)
#define STENCIL_BUFFER_PIXEL_OFFSET (MAX_RENDER_PIXELS*3)
#define ACCUM1_BUFFER_PIXEL_OFFSET  (MAX_RENDER_PIXELS*4)
#define ACCUM2_BUFFER_PIXEL_OFFSET  (MAX_RENDER_PIXELS*5)

typedef float    ZType;
typedef u8       StencilType;
typedef u32      ColorType;
/*
    Surface equation solver
*/
struct PlaneStepper3
{
    float ddx, ddy;
    float c;

    void Setup(taRECT *rect, const Vertex& v1, const Vertex& v2, const Vertex& v3, float v1_a, float v2_a, float v3_a)
    {
        float Aa = ((v3_a - v1_a) * (v2.y - v1.y) - (v2_a - v1_a) * (v3.y - v1.y));
        float Ba = ((v3.x - v1.x) * (v2_a - v1_a) - (v2.x - v1.x) * (v3_a - v1_a));

        float C = ((v2.x - v1.x) * (v3.y - v1.y) - (v3.x - v1.x) * (v2.y - v1.y));
        
        ddx = -Aa / C;
        ddy = -Ba / C;

        c = v1_a - ddx * (v1.x - rect->left) - ddy * (v1.y - rect->top);
    }

    float Ip(float x, float y) const
    {
        return x * ddx + y * ddy + c;
    }

    float Ip(float x, float y, float W) const
    {
        return Ip(x, y) * W;
    }
};

/*
    Interpolation helper
*/
struct IPs3
{
    PlaneStepper3 U;
    PlaneStepper3 V;
    PlaneStepper3 Col[4];
    PlaneStepper3 Ofs[4];

    void Setup(taRECT *rect, DrawParameters* params, text_info* texture, const Vertex& v1, const Vertex& v2, const Vertex& v3)
    {
        u32 w = 0, h = 0;
        if (texture) {
            w = texture->width;
            h = texture->height;
        }

        U.Setup(rect, v1, v2, v3, v1.u * w * v1.z, v2.u * w * v2.z, v3.u * w * v3.z);
        V.Setup(rect, v1, v2, v3, v1.v * h * v1.z, v2.v * h * v2.z, v3.v * h * v3.z);
        if (params->isp.Gouraud) {
            for (int i = 0; i < 4; i++)
                Col[i].Setup(rect, v1, v2, v3, v1.col[i] * v1.z, v2.col[i] * v2.z, v3.col[i] * v3.z);

            for (int i = 0; i < 4; i++)
                Ofs[i].Setup(rect, v1, v2, v3, v1.spc[i] * v1.z, v2.spc[i] * v2.z, v3.spc[i] * v3.z);
        } else {
            for (int i = 0; i < 4; i++)
                Col[i].Setup(rect, v1, v2, v3, v3.col[i] * v1.z, v3.col[i] * v2.z, v3.col[i] * v3.z);

            for (int i = 0; i < 4; i++)
                Ofs[i].Setup(rect, v1, v2, v3, v3.spc[i] * v1.z, v3.spc[i] * v2.z, v3.spc[i] * v3.z);
        }
    }
};

// Used for deferred TSP processing lookups
struct FpuEntry
{
    IPs3 ips;
    DrawParameters params;
    text_info texture;
};

union Color {
    u32 raw;
    u8 bgra[4];
    struct {
        u8 b;
        u8 g;
        u8 r;
        u8 a;
    };
};
// Used by layer peeling to determine end of processing
extern int PixelsDrawn;

extern u32 render_buffer[MAX_RENDER_PIXELS * 6]; //param pointers + depth1 + depth2 + stencil + acum 1 + acum 2
extern parameter_tag_t tagBuffer    [MAX_RENDER_PIXELS];
extern StencilType     stencilBuffer[MAX_RENDER_PIXELS];
extern u32             colorBuffer1 [MAX_RENDER_PIXELS];
extern u32             colorBuffer2 [MAX_RENDER_PIXELS];
extern ZType           depthBuffer1 [MAX_RENDER_PIXELS];
extern ZType           depthBuffer2 [MAX_RENDER_PIXELS];

void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue);
void ClearParamBuffer(parameter_tag_t paramValue);
void PeelBuffers(float depthValue, u32 stencilValue);
void SummarizeStencilOr();
void SummarizeStencilAnd();
void ClearPixelsDrawn();
u32 GetPixelsDrawn();

// Render to ACCUM from TAG buffer
// TAG holds references to triangles, ACCUM is the tile framebuffer
void RenderParamTags(RenderMode rm, int tileX, int tileY);
void ClearFpuEntries();

f32 f16(u16 v);

//decode a vertex in the native pvr format
void decode_pvr_vertex(DrawParameters* params, pvr32addr_t ptr,Vertex* cv);
// decode an object (params + vertexes)
u32 decode_pvr_vetrices(DrawParameters* params, pvr32addr_t base, u32 skip, u32 shadow, Vertex* vtx, int count, int offset);

FpuEntry GetFpuEntry(taRECT *rect, RenderMode render_mode, ISP_BACKGND_T_type core_tag);
// Lookup/create cached TSP parameters, and call PixelFlush_tsp
bool PixelFlush_tsp(bool pp_AlphaTest, FpuEntry* entry, float x, float y, u8 *rb, float invW);
// Rasterize a single triangle to ISP (or ISP+TSP for PT)
void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area);
u8* GetColorOutputBuffer();
u8* DebugGetAllBuffers();

void operator delete(void* p);

// Implement the full texture/shade pipeline for a pixel
bool PixelFlush_tsp(
    bool pp_UseAlpha, bool pp_Texture, bool pp_Offset, bool pp_ColorClamp, u32 pp_FogCtrl, bool pp_IgnoreAlpha, bool pp_ClampU, bool pp_ClampV, bool pp_FlipU, bool pp_FlipV, u32 pp_FilterMode, u32 pp_ShadInstr, bool pp_AlphaTest, u32 pp_SrcSel, u32 pp_DstSel, u32 pp_SrcInst, u32 pp_DstInst,
    const FpuEntry *entry, float x, float y, float W, u8 *rb);

// Depth processing for a pixel -- render_mode 0: OPAQ, 1: PT, 2: TRANS
void PixelFlush_isp(RenderMode render_mode, u32 depth_mode, int pixIdx, float x, float y, float invW, u8 *pb, ZType* zb, parameter_tag_t tag);



/*
    Main renderer class
*/

void RenderTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area);
// called on vblank
bool RenderFramebuffer();
u32 ReadRegionArrayEntry(u32 base, RegionArrayEntry* entry);
f32 f16(u16 v);
void decode_pvr_vertex(DrawParameters* params, pvr32addr_t ptr,Vertex* cv);
u32 decode_pvr_vertices(DrawParameters* params, pvr32addr_t base, u32 skip, u32 shadow, Vertex* vtx, int count);
ISP_BACKGND_T_type CoreTagFromDesc(u32 cache_bypass, u32 shadow, u32 skip, u32 tag_address, u32 tag_offset);
void RenderTriangleStrip(RenderMode render_mode, ObjectListEntry obj, taRECT* rect);
void RenderTriangleArray(RenderMode render_mode, ObjectListEntry obj, taRECT* rect);
void RenderQuadArray(RenderMode render_mode, ObjectListEntry obj, taRECT* rect);
void RenderObjectList(RenderMode render_mode, pvr32addr_t base, taRECT* rect);
bool RenderPVR();
void Present();