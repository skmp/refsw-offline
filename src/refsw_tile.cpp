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
#include "refsw_pixel.h"

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

struct refsw_impl : refsw
{
    u32 render_buffer[MAX_RENDER_PIXELS * 6]; //param pointers + depth1 + depth2 + stencil + acum 1 + acum 2
    unique_ptr<RefPixelPipeline> pixelPipeline;

    u8* vram;
    refsw_impl(u8* vram, RefPixelPipeline* pixelPipeline) : vram(vram), pixelPipeline(pixelPipeline) {
        
    }

    bool Init() {
        return true;
    }

    void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue)
    {
        auto zb = reinterpret_cast<float*>(render_buffer + DEPTH1_BUFFER_PIXEL_OFFSET);
        auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);
        auto pb = reinterpret_cast<parameter_tag_t*>(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            zb[i] = depthValue;
            stencil[i] = stencilValue;
            pb[i] = paramValue;
        }
    }

    void ClearParamBuffer(parameter_tag_t paramValue) {
        auto pb = reinterpret_cast<parameter_tag_t*>(render_buffer + PARAM_BUFFER_PIXEL_OFFSET);

        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            pb[i] = paramValue;
        }
    }

    void PeelBuffers(float depthValue, u32 stencilValue)
    {
        auto zb = reinterpret_cast<float*>(render_buffer + DEPTH1_BUFFER_PIXEL_OFFSET);
        auto zb2 = reinterpret_cast<float*>(render_buffer + DEPTH2_BUFFER_PIXEL_OFFSET);
        auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);

        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            zb2[i] = zb[i];     // keep old ZB for reference
            zb[i] = depthValue;    // set the "closest" test to furthest value possible
            stencil[i] = stencilValue;
        }
    }

    void SummarizeStencilOr() {
        auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);

        // post movdol merge INSIDE
        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            stencil[i] |= (stencil[i] >>1);
        }
    }

    void SummarizeStencilAnd() {
        auto stencil = reinterpret_cast<u32*>(render_buffer + STENCIL_BUFFER_PIXEL_OFFSET);

        // post movdol merge OUTSIDE
        for (int i = 0; i < MAX_RENDER_PIXELS; i++) {
            stencil[i] &= (stencil[i] >>1);
        }
    }

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
    void RenderParamTags(RenderMode rm, int tileX, int tileY) {

        auto rb = render_buffer;
        float halfpixel = HALF_OFFSET.tsp_pixel_half_offset ? 0.5f : 0;
        taRECT rect;
        rect.left = tileX;
        rect.top = tileY;
        rect.bottom = rect.top + 32;
        rect.right = rect.left + 32;

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                auto tag =  *(parameter_tag_t*)&rb[PARAM_BUFFER_PIXEL_OFFSET];
                if (!(tag & TAG_INVALID)) {
                    ISP_BACKGND_T_type t;
                    t.full = tag;
                    auto Entry = GetFpuEntry(&rect, rm, t);
                    PixelFlush_tsp(rm == RM_PUNCHTHROUGH, &Entry, x + halfpixel, y + halfpixel, (u8*)rb,  *(f32*)&rb[DEPTH1_BUFFER_PIXEL_OFFSET]);
                }
                rb++;
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
    void decode_pvr_vertex(DrawParameters* params, pvr32addr_t ptr,Vertex* cv)
    {
        //XYZ
        //UV
        //Base Col
        //Offset Col

        //XYZ are _allways_ there :)
        cv->x=vrf(vram, ptr);ptr+=4;
        cv->y=vrf(vram, ptr);ptr+=4;
        cv->z=vrf(vram, ptr);ptr+=4;

        if (params->isp.Texture)
        {	//Do texture , if any
            if (params->isp.UV_16b)
            {
                u32 uv=vri(vram, ptr);
                cv->u = f16((u16)(uv >>16));
                cv->v = f16((u16)(uv >> 0));
                ptr+=4;
            }
            else
            {
                cv->u=vrf(vram, ptr);ptr+=4;
                cv->v=vrf(vram, ptr);ptr+=4;
            }
        }

        //Color
        u32 col=vri(vram, ptr);ptr+=4;
        vert_packed_color_(cv->col,col);
        if (params->isp.Offset)
        {
            //Intensity color
            u32 col=vri(vram, ptr);ptr+=4;
            vert_packed_color_(cv->spc,col);
        }
    }

    // decode an object (params + vertexes)
    u32 decode_pvr_vetrices(DrawParameters* params, pvr32addr_t base, u32 skip, u32 shadow, Vertex* vtx, int count, int offset)
    {
        bool PSVM=FPU_SHAD_SCALE.intensity_shadow == 0;

        if (!PSVM) {
            shadow = 0; // no double volume stuff
        }

        params->isp.full=vri(vram, base);
        params->tsp.full=vri(vram, base+4);
        params->tcw.full=vri(vram, base+8);

        base += 12;
        if (shadow) {
            params->tsp2.full=vri(vram, base+0);
            params->tcw2.full=vri(vram, base+4);
            base += 8;
        }

        for (int i = 0; i < offset; i++) {
            base += (3 + skip * (shadow+1)) * 4;
        }

        for (int i = 0; i < count; i++) {
            decode_pvr_vertex(params,base, &vtx[i]);
            base += (3 + skip * (shadow+1)) * 4;
        }

        return base;
    }

    FpuEntry GetFpuEntry(taRECT *rect, RenderMode render_mode, ISP_BACKGND_T_type core_tag)
    {
        FpuEntry entry;
        Vertex vtx[3];
        decode_pvr_vetrices(&entry.params, PARAM_BASE + core_tag.tag_address * 4, core_tag.skip, core_tag.shadow, vtx, 3, core_tag.tag_offset);
        // generate
        if (entry.params.isp.Texture)
        {
            entry.texture = raw_GetTexture(vram, entry.params.tsp, entry.params.tcw);
        }

        entry.ips.Setup(rect, &entry.params, &entry.texture, vtx[0], vtx[1], vtx[2]);

        // entry.tsp = pixelPipeline->PixelFlush_tsp;
        // entry.textureFetch = pixelPipeline->TextureFetch;
        // entry.colorCombiner = pixelPipeline->ColorCombiner; // GetColorCombiner(entry.params.isp, entry.params.tsp);
        // entry.blendingUnit = pixelPipeline->BlendingUnit;   // GetBlendingUnit(render_mode, entry.params.tsp);

        return entry;
    }

    // Lookup/create cached TSP parameters, and call PixelFlush_tsp
    bool PixelFlush_tsp(bool pp_AlphaTest, FpuEntry* entry, float x, float y, u8 *rb, float invW)
    {   
        return pixelPipeline->PixelFlush_tsp(entry->params.tsp.UseAlpha, entry->params.isp.Texture, entry->params.isp.Offset, entry->params.tsp.ColorClamp, entry->params.tsp.FogCtrl,
                                            entry->params.tsp.IgnoreTexA, entry->params.tsp.ClampU, entry->params.tsp.ClampV, entry->params.tsp.FlipU,  entry->params.tsp.FlipV,  entry->params.tsp.FilterMode,  entry->params.tsp.ShadInstr,  
                                            pp_AlphaTest,  entry->params.tsp.SrcSelect,  entry->params.tsp.DstSelect,  entry->params.tsp.SrcInstr,  entry->params.tsp.DstInstr,
                                            entry, x, y, 1/invW, rb);
    }

    // Rasterize a single triangle to ISP (or ISP+TSP for PT)
    void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area)
    {
        const int stride_bytes = STRIDE_PIXEL_OFFSET * 4;
        //Plane equation

#define FLUSH_NAN(a) isnan(a) ? 0 : a

        const float Y1 = FLUSH_NAN(v1.y);
        const float Y2 = FLUSH_NAN(v2.y);
        const float Y3 = FLUSH_NAN(v3.y);
        const float Y4 = v4 ? FLUSH_NAN(v4->y) : 0;

        const float X1 = FLUSH_NAN(v1.x);
        const float X2 = FLUSH_NAN(v2.x);
        const float X3 = FLUSH_NAN(v3.x);
        const float X4 = v4 ? FLUSH_NAN(v4->x) : 0;

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

        // Bounding rectangle
        int minx = mmin(X1, X2, X3, area->left);
        int miny = mmin(Y1, Y2, Y3, area->top);

        int spanx = mmax(X1+1, X2+1, X3+1, area->right - 1) - minx + 1;
        int spany = mmax(Y1+1, Y2+1, Y3+1, area->bottom - 1) - miny + 1;

        //Inside scissor area?
        if (spanx < 0 || spany < 0)
            return;

        // Half-edge constants
        const float DX12 = sgn * (X1 - X2);
        const float DX23 = sgn * (X2 - X3);
        const float DX31 = v4 ? sgn * (X3 - X4) : sgn * (X3 - X1);
        const float DX41 = v4 ? sgn * (X4 - X1) : 0;

        const float DY12 = sgn * (Y1 - Y2);
        const float DY23 = sgn * (Y2 - Y3);
        const float DY31 = v4 ? sgn * (Y3 - Y4) : sgn * (Y3 - Y1);
        const float DY41 = v4 ? sgn * (Y4 - Y1) : 0;

        float C1 = DY12 * (X1 - area->left) - DX12 * (Y1 - area->top);
        float C2 = DY23 * (X2 - area->left) - DX23 * (Y2 - area->top);
        float C3 = DY31 * (X3 - area->left) - DX31 * (Y3 - area->top);
        float C4 = v4 ? DY41 * (X4 + area->left) - DX41 * (Y4 + area->top) : 1;


        u8* cb_y = (u8*)render_buffer;
        cb_y += (miny - area->top) * stride_bytes + (minx - area->left) * 4;

        PlaneStepper3 Z;
        Z.Setup(area, v1, v2, v3, v1.z, v2.z, v3.z);

        float halfpixel = HALF_OFFSET.fpu_pixel_half_offset ? 0.5f : 0;
        float y_ps = miny - area->top + halfpixel;
        float minx_ps = minx - area->left + halfpixel;

        //auto pixelFlush = pixelPipeline->GetIsp(render_mode, params->isp);

        // Loop through pixels
        for (int y = spany; y > 0; y -= 1)
        {
            u8* cb_x = cb_y;
            float x_ps = minx_ps;
            for (int x = spanx; x > 0; x -= 1)
            {
                float Xhs12 = C1 + DX12 * y_ps - DY12 * x_ps;
                float Xhs23 = C2 + DX23 * y_ps - DY23 * x_ps;
                float Xhs31 = C3 + DX31 * y_ps - DY31 * x_ps;
                float Xhs41 = C4 + DX41 * y_ps - DY41 * x_ps;

                bool inTriangle = Xhs12 >= 0 && Xhs23 >= 0 && Xhs31 >= 0 && Xhs41 >= 0;

                if (inTriangle)
                {
                    float invW = Z.Ip(x_ps, y_ps);
                    pixelPipeline->PixelFlush_isp(this, render_mode, params->isp.DepthMode, x_ps, y_ps, invW, cb_x, tag);
                }

                cb_x += 4;
                x_ps = x_ps + 1;
            }
        next_y:
            cb_y += stride_bytes;
            y_ps = y_ps + 1;
        }
    }
    
    virtual u8* GetColorOutputBuffer() {
        return reinterpret_cast<u8*>(render_buffer + ACCUM1_BUFFER_PIXEL_OFFSET);
    }

    virtual u8* DebugGetAllBuffers() {
        return reinterpret_cast<u8*>(render_buffer);
    }

    void operator delete(void* p) {
        free(p);
    }
};

Renderer* rend_refsw(u8* vram) {
    return rend_refred_base(vram, [=]() { 
        return (RefRendInterface*) new refsw_impl(vram, new RefPixelPipeline());
    });
}
