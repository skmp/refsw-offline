#pragma once
/*
	This file is part of libswirl
*/
#include "license/bsd"

#include <functional>
#include "core_structs.h" // for ISP_TSP and co
#include "refsw_lists_regs.h"

struct RegionArrayEntry {
    RegionArrayEntryControl control;
    ListPointer opaque;
    ListPointer opaque_mod;
    ListPointer trans;
    ListPointer trans_mod;
    ListPointer puncht;
};

struct DrawParameters
{
    ISP_TSP isp;
    TSP tsp;
    TCW tcw;
    TSP tsp2;
    TSP tcw2;
};

enum RenderMode {
    RM_OPAQUE,
    RM_PUNCHTHROUGH,
    RM_TRANSLUCENT,
    RM_MODIFIER,
    RM_COUNT
};

#define TAG_INVALID 1

typedef u32 parameter_tag_t;

struct taRECT {
    int left, top, right, bottom;
};

struct RefRendInterface
{
    // backend specific init
    virtual bool Init() = 0;

    // Clear the buffers
    virtual void ClearBuffers(u32 paramValue, float depthValue, u32 stencilValue) = 0;

    // Clear only the param buffer (used for peeling)
    virtual void ClearParamBuffer(u32 paramValue) = 0;

    // Clear and set DEPTH2 for peeling
    virtual void PeelBuffers(float depthValue, u32 stencilValue) = 0;

    // Summarize tile after rendering modvol (inside)
    virtual void SummarizeStencilOr() = 0;
    
    // Summarize tile after rendering modvol (outside)
    virtual void SummarizeStencilAnd() = 0;

    // Clear the pixel drawn counter
    virtual void ClearPixelsDrawn() = 0;

    // Get the pixel drawn counter. Used during layer peeling to determine when to stop processing
    virtual u32 GetPixelsDrawn() = 0;

    // Add an entry to the fpu parameters list
    virtual parameter_tag_t AddFpuEntry(taRECT *rect, DrawParameters* params, Vertex* vtx, RenderMode render_mode, ISP_BACKGND_T_type core_tag) = 0;

    // Clear the fpu parameters list
    virtual void ClearFpuEntries() = 0;

    // Get the final output of the 32x32 tile. Used to write to the VRAM framebuffer
    virtual u8* GetColorOutputBuffer() = 0;

    // Render to ACCUM from TAG buffer
    // TAG holds references to triangles, ACCUM is the tile framebuffer
    virtual void RenderParamTags(RenderMode rm, int tileX, int tileY) = 0;

    // RasterizeTriangle
    virtual void RasterizeTriangle(RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area) = 0;

    virtual ~RefRendInterface() { };

    // Debug-level
    virtual u8* DebugGetAllBuffers() = 0;

    virtual void DebugOnFrameStart(int fn) { }
    virtual void DebugOnTileStart(int x, int y) { }
};

Renderer* rend_refred_base(u8* vram, function<RefRendInterface*()> createBackend);

RefRendInterface*  rend_refred_debug(RefRendInterface* backend);
