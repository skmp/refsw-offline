/*
	This file is part of libswirl
*/
//#include "license/bsd"

/*
    REFSW: Reference-style software rasterizer

    An attempt to model CLX2's CORE/SPG/RAMDAC at the lowest functional level

    Rasterizer structure
    ===

    Reads tile lists in CORE format, generated from a LLE TA implementation or software running from sh4,
    renders them in 32x32 tiles, reads out to VRAM and displays framebuffer from VRAM.

    CORE high level overview
    ===

    CORE Renders based on the REGION ARRAY, which is a flag-terminated list of tiles. Each RegionArrayEntry
    contains the TILE x/y position, control flags for Z clear/Write out/Presort and pointers to OBJECT LISTS.

    OBJECT LISTS are inline linked lists containing ObjectListEntries. Each ObjectListEntry has a small
    descriptor for the entry type and vertex size, and a pointer to the OBJECT DATA.

    OBJECT DATA contains the PARAMETERS for the OBJECT (ISP, TSP, TCW, optional TSP2 and TCW2) and vertixes.

    There are 3 OBJECT DATA TYPES
    - Triangle Strips (PARAMETERS, up to 8 VTXs) x 1
    - Triangle Arrays (PARAMETERS, 3 vtx) x Num_of_primitives
    - Quad Arrays (PARAMETERS, 4 vtx) x Num_of_primitives

    CORE renders the OBJECTS to its internal TILE BUFFERS, scales and filters the output (SCL)
    and writes out to VRAM.

    CORE Rendering details
    ===

    CORE has four main components, FPU (triangle setup) ISP (Rasterization, depth, stencil), TSP (Texutre + Shading)
    and SCL (tile writeout + scaling). There are three color rendering modes: DEPTH FIRST, DEPTH + COLOR and LAYER PEELING.

    OPAQUE OBJECTS are rendered using the DEPTH FIRST mode.
    PUNCH THROUGH OBJECTS are rendered using the DEPTH + COLOR mode.
    TRANSPARENT OBJECTS are rendered using either the DEPTH + COLOR mode or the LAYER PEELING mode.
    
    DEPTH FIRST mode
    ---
    OBJECTS are first rendered by ISP in the depth and tag buffers, 32 pixels (?) at a time. then the SPAN SORTER collects spans with the
    same tag and sends them to TSP for shading processing, one pixel at a time.

    DEPTH + COLOR mode
    ---
    OBJECTS are rendered by ISP and TSP at the same time, one pixel (?) at a time. ALPHA TEST feedback from TSP modifies the Z-write behavior.

    LAYER PEELING mode
    ---

    OBJECTS are first rendered by ISP in the depth and tag buffers, using a depth pass and a depth test buffer. SPAN SORTER collects spans with
    the same tag and sends them to TSP for shading processing. The process repeats itself until all layers have been indepedently rendered. On
    each pass, only the pixels with the lowest depth value that pass the depth pass buffer are rendered. In case of identical depth values, the
    tag buffer is used to sort the pixels by tag as well as depth in order to support co-planar polygons.
*/

#include <omp.h>
#include "Renderer_if.h"
#include "pvr_mem.h"
#include "oslib.h"
#include "TexCache.h"
#include "TexConv.h"  // for pixel buffer, used for presenting

#include <png.h>

#include <cmath>
#include <float.h>

#include <memory>

#include "pvr_regs.h"

#if HOST_OS == OS_WINDOWS
static BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), 0, 0, 1, 32, BI_RGB };
#endif

#include "refsw_lists.h"


/*
    Main renderer class
*/
struct refrend : Renderer
{
    std::function<RefRendInterface*()> createBackend;

    RefRendInterface* backend;

    u8* vram;

    int numRenders = 0;

    refrend(u8* vram, function<RefRendInterface*()> createBackend) : vram(vram), createBackend(createBackend) {
        backend = createBackend();

        backend->Init();
    }

    void RenderTriangle(RefRendInterface* backend, RenderMode render_mode, DrawParameters* params, parameter_tag_t tag, int vertex_offset, const Vertex& v1, const Vertex& v2, const Vertex& v3, const Vertex* v4, taRECT* area)
    {
        backend->RasterizeTriangle(render_mode, params, tag, vertex_offset, v1, v2, v3, v4, area);

        if (render_mode == RM_MODIFIER)
        {          
            if (params->isp.modvol.VolumeMode == 1 ) 
            {
                backend->SummarizeStencilOr();
            }
            else if (params->isp.modvol.VolumeMode == 2) 
            {
                backend->SummarizeStencilAnd();
            }
        }
    }

    // called on vblank
    virtual bool RenderFramebuffer() {
        Present();
        return false;
    }

    u32 ReadRegionArrayEntry(u32 base, RegionArrayEntry* entry) 
    {
        bool fmt_v1 = (((FPU_PARAM_CFG >> 21) & 1) == 0);

        entry->control.full     = vri(vram, base);
        entry->opaque.full      = vri(vram, base + 4);
        entry->opaque_mod.full  = vri(vram, base + 8);
        entry->trans.full       = vri(vram, base + 12);
        entry->trans_mod.full   = vri(vram, base + 16);

        if (fmt_v1)
        {
            entry->puncht.full = 0x80000000;
            return 5 * 4;
        }
        else
        {
            entry->puncht.full = vri(vram, base + 20);
            return 6 * 4;
        }
    }

    f32 f16(u16 v)
    {
        u32 z=v<<16;
        return *(f32*)&z;
    }

	#define vert_packed_color_(to,src) \
		{ \
		u32 t=src; \
		to[2] = (u8)(t);t>>=8;\
		to[1] = (u8)(t);t>>=8;\
		to[0] = (u8)(t);t>>=8;\
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
    u32 decode_pvr_vetrices(DrawParameters* params, pvr32addr_t base, u32 skip, u32 shadow, Vertex* vtx, int count)
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

        for (int i = 0; i < count; i++) {
            decode_pvr_vertex(params,base, &vtx[i]);
            base += (3 + skip * (shadow+1)) * 4;
        }

        return base;
    }

    ISP_BACKGND_T_type CoreTagFromDesc(u32 cache_bypass, u32 shadow, u32 skip, u32 tag_address, u32 tag_offset) {
        ISP_BACKGND_T_type rv;
        
        rv.cache_bypass = cache_bypass;
        rv.shadow = shadow;
        rv.skip = skip;
        rv.tag_address = tag_address;
        rv.tag_offset = tag_offset;

        return rv;
    }

    // render a triangle strip object list entry
    void RenderTriangleStrip(RefRendInterface* backend, RenderMode render_mode, ObjectListEntry obj, taRECT* rect)
    {
        Vertex vtx[8];
        DrawParameters params;

        u32 param_base = PARAM_BASE & 0xF00000;

        u32 tag_address = param_base + obj.tstrip.param_offs_in_words * 4;

        decode_pvr_vetrices(&params, tag_address, obj.tstrip.skip, obj.tstrip.shadow, vtx, 8);


        for (int i = 0; i < 6; i++)
        {
            if (obj.tstrip.mask & (1 << (5-i)))
            {

                auto core_tag = CoreTagFromDesc(params.isp.CacheBypass, obj.tstrip.shadow, obj.tstrip.skip, tag_address, i);
                parameter_tag_t tag = backend->AddFpuEntry(&params, &vtx[i], render_mode, core_tag);

                RenderTriangle(backend, render_mode, &params, tag, i&1, vtx[i+0], vtx[i+1], vtx[i+2], nullptr, rect);
            }
        }
    }


    // render a triangle array object list entry
    void RenderTriangleArray(RefRendInterface* backend, RenderMode render_mode, ObjectListEntry obj, taRECT* rect)
    {
        auto triangles = obj.tarray.prims + 1;
        u32 param_base = PARAM_BASE & 0xF00000;


        u32 param_ptr = param_base + obj.tarray.param_offs_in_words * 4;
        
        for (int i = 0; i<triangles; i++)
        {
            DrawParameters params;
            Vertex vtx[3];

            u32 tag_address = param_ptr;
            param_ptr = decode_pvr_vetrices(&params, tag_address, obj.tarray.skip, obj.tarray.shadow, vtx, 3);
            
            parameter_tag_t tag = 0;
            if (render_mode != RM_MODIFIER)
            {
                auto core_tag = CoreTagFromDesc(params.isp.CacheBypass, obj.tstrip.shadow, obj.tstrip.skip, tag_address, 0);
                tag = backend->AddFpuEntry(&params, &vtx[0], render_mode, core_tag);
            }

            RenderTriangle(backend, render_mode, &params, tag, 0, vtx[0], vtx[1], vtx[2], nullptr, rect);
        }
    }

    // render a quad array object list entry
    void RenderQuadArray(RefRendInterface* backend, RenderMode render_mode, ObjectListEntry obj, taRECT* rect)
    {
        auto quads = obj.qarray.prims + 1;
        u32 param_base = PARAM_BASE & 0xF00000;


        u32 param_ptr = param_base + obj.qarray.param_offs_in_words * 4;
        
        for (int i = 0; i<quads; i++)
        {
            DrawParameters params;
            Vertex vtx[4];

            u32 tag_address = param_ptr;
            param_ptr = decode_pvr_vetrices(&params, tag_address, obj.qarray.skip, obj.qarray.shadow, vtx, 4);
            
            auto core_tag = CoreTagFromDesc(params.isp.CacheBypass, obj.tstrip.shadow, obj.tstrip.skip, tag_address, 0);
            parameter_tag_t tag = backend->AddFpuEntry(&params, &vtx[0], render_mode, core_tag);

            //TODO: FIXME
            RenderTriangle(backend, render_mode, &params, tag, 0, vtx[0], vtx[1], vtx[2], &vtx[3], rect);
        }
    }

    // Render an object list
    void RenderObjectList(RefRendInterface* backend, RenderMode render_mode, pvr32addr_t base, taRECT* rect)
    {
        ObjectListEntry obj;

        for (;;) {
            obj.full = vri(vram, base);
            base += 4;

            if (!obj.is_not_triangle_strip) {
                RenderTriangleStrip(backend, render_mode, obj, rect);
            } else {
                switch(obj.type) {
                    case 0b111: // link
                        if (obj.link.end_of_list)
                            return;

                            base = obj.link.next_block_ptr_in_words * 4;
                        break;

                    case 0b100: // triangle array
                        RenderTriangleArray(backend, render_mode, obj, rect);
                        break;
                    
                    case 0b101: // quad array
                        RenderQuadArray(backend, render_mode, obj, rect);
                        break;

                    default:
                        printf("RenderObjectList: Not handled object type: %d\n", obj.type);
                }
            }
        }
    }


    // Render a frame
    // Called on START_RENDER write
    virtual bool RenderPVR() {
        numRenders++;

        palette_update();

        u32 base = REGION_BASE;

        RegionArrayEntry entry;

        backend->DebugOnFrameStart(numRenders);
        
        // Parse region array
        do {
            base += ReadRegionArrayEntry(base, &entry);
            
            taRECT rect;
            rect.top = entry.control.tiley * 32;
            rect.left = entry.control.tilex * 32;

            rect.bottom = rect.top + 32;
            rect.right = rect.left + 32;

            parameter_tag_t bgTag;

            backend->DebugOnTileStart(rect.left, rect.top);

            // register BGPOLY to fpu
            {
                DrawParameters params;
                Vertex vtx[8];
                decode_pvr_vetrices(&params, PARAM_BASE + ISP_BACKGND_T.tag_address * 4, ISP_BACKGND_T.skip, ISP_BACKGND_T.shadow, vtx, 8);
                bgTag = backend->AddFpuEntry(&params, &vtx[ISP_BACKGND_T.tag_offset], RM_OPAQUE, ISP_BACKGND_T);
            }

            // Tile needs clear?
            if (!entry.control.z_keep)
            {
                // Clear Param + Z + stencil buffers
                backend->ClearBuffers(bgTag, ISP_BACKGND_D.f, 0);
            }

            // Render OPAQ to TAGS
            if (!entry.opaque.empty)
            {
                RenderObjectList(backend, RM_OPAQUE, entry.opaque.ptr_in_words * 4, &rect);
            }

            // render PT to TAGS
            if (!entry.puncht.empty)
            {
                RenderObjectList(backend, RM_PUNCHTHROUGH, entry.puncht.ptr_in_words * 4, &rect);
            }

            //TODO: Render OPAQ modvols
            if (!entry.opaque_mod.empty)
            {
                RenderObjectList(backend, RM_MODIFIER, entry.opaque_mod.ptr_in_words * 4, &rect);
            }

            // Render TAGS to ACCUM
            backend->RenderParamTags(RM_OPAQUE, rect.left, rect.top);

            // layer peeling rendering
            if (!entry.trans.empty)
            {
                // clear the param buffer
                backend->ClearParamBuffer(TAG_INVALID);

                do
                {
                    // prepare for a new pass
                    backend->ClearPixelsDrawn();

                    // copy depth test to depth reference buffer, clear depth test buffer, clear stencil
                    backend->PeelBuffers(FLT_MAX, 0);

                    // render to TAGS
                    RenderObjectList(backend, RM_TRANSLUCENT, entry.trans.ptr_in_words * 4, &rect);

                    if (!entry.trans_mod.empty)
                    {
                        RenderObjectList(backend, RM_MODIFIER, entry.trans_mod.ptr_in_words * 4, &rect);
                    }

                    // render TAGS to ACCUM
                    // also marks TAGS as invalid, but keeps the index for coplanar sorting
                    backend->RenderParamTags(RM_TRANSLUCENT, rect.left, rect.top);
                } while (backend->GetPixelsDrawn() != 0);
            }

            // Copy to vram
            if (!entry.control.no_writeout)
            {
                auto copy = new u8[32 * 32 * 4];
                memcpy(copy, backend->GetColorOutputBuffer(), 32 * 32 * 4);

                auto field = SCALER_CTL.fieldselect;
                auto interlace = SCALER_CTL.interlace;

                auto base = (interlace && field) ? FB_W_SOF2 : FB_W_SOF1;

                // very few configurations supported here
                verify(SCALER_CTL.hscale == 0);
                verify(SCALER_CTL.interlace == 0); // write both SOFs
                auto vscale = SCALER_CTL.vscalefactor;
                verify(vscale == 0x401 || vscale == 0x400 || vscale == 0x800);

                auto fb_packmode = FB_W_CTRL.fb_packmode;
                verify(fb_packmode == 0x1); // 565 RGB16

                auto src = copy;
                auto bpp = 2;
                auto offset_bytes = entry.control.tilex * 32 * bpp + entry.control.tiley * 32 * FB_W_LINESTRIDE.stride * 8;

                for (int y = 0; y < 32; y++)
                {
                    //auto base = (y&1) ? FB_W_SOF2 : FB_W_SOF1;
                    auto dst = base + offset_bytes + (y)*FB_W_LINESTRIDE.stride * 8;

                    for (int x = 0; x < 32; x++)
                    {
                        auto pixel = (((src[0] >> 3) & 0x1F) << 0) | (((src[1] >> 2) & 0x3F) << 5) | (((src[2] >> 3) & 0x1F) << 11);
                        pvr_write_area1_16(vram, dst, pixel);

                        dst += bpp;
                        src += 4; // skip alpha
                    }
                }

                delete[] copy;
            }

            // clear the tsp cache
            backend->ClearFpuEntries();
        } while (!entry.control.last_region);

        return false;
    }

#if HOST_OS == OS_WINDOWS
    HWND hWnd;
    HBITMAP hBMP = 0, holdBMP;
    HDC hmem;
#endif


    virtual bool Init() {

        return true;
    }

    ~refrend() {
        if (backend) {
            delete backend;
            backend = nullptr;
        }
    }

    virtual void Present()
    {

        if (FB_R_SIZE.fb_x_size == 0 || FB_R_SIZE.fb_y_size == 0)
            return;

        int width = (FB_R_SIZE.fb_x_size + 1) << 1; // in 16-bit words
        int height = FB_R_SIZE.fb_y_size + 1;
        int modulus = (FB_R_SIZE.fb_modulus - 1) << 1;

        int bpp;
        switch (FB_R_CTRL.fb_depth)
        {
        case fbde_0555:
        case fbde_565:
            bpp = 2;
            break;
        case fbde_888:
            bpp = 3;
            width = (width * 2) / 3;     // in pixels
            modulus = (modulus * 2) / 3; // in pixels
            break;
        case fbde_C888:
            bpp = 4;
            width /= 2;   // in pixels
            modulus /= 2; // in pixels
            break;
        default:
            die("Invalid framebuffer format\n");
            bpp = 4;
            break;
        }

        #if !defined(REFSW_OFFLINE)
        u32 addr = SPG_CONTROL.interlace && SPG_STATUS.fieldnum ? FB_R_SOF2 : FB_R_SOF1;
        #else
        u32 addr = FB_W_SOF1;
        #endif

        static PixelBuffer<u32> pb;
        if (pb.total_pixels != width * (SPG_CONTROL.interlace ? (height * 2 + 1) : height)) {
            pb.init(width, SPG_CONTROL.interlace ? (height * 2 + 1) : height);
        }

        u8 *dst = (u8 *)pb.data();

        if (SPG_CONTROL.interlace & SPG_STATUS.fieldnum) {
            dst += width * 4;
        }

#if !defined(REFSW_OFFLINE)
#define RED_5 0
#define BLUE_5 10
#define RED_6 0
#define BLUE_6 11
#define RED_8 0
#define BLUE_8 16
#else
#define RED_5 10
#define BLUE_5 0
#define RED_6 11
#define BLUE_6 0
#define RED_8 16
#define BLUE_8 0
#endif

        switch (FB_R_CTRL.fb_depth)
        {
        case fbde_0555: // 555 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    u16 src = pvr_read_area1_16(vram, addr);
                    *dst++ = (((src >> RED_5) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = (((src >> 5) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = (((src >> BLUE_5) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;
                if (SPG_CONTROL.interlace) {
                    dst += width * 4;
                }
            }
            break;

        case fbde_565: // 565 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    u16 src = pvr_read_area1_16(vram, addr);
                    *dst++ = (((src >> RED_6) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = (((src >> 5) & 0x3F) << 2) + (FB_R_CTRL.fb_concat >> 1);
                    *dst++ = (((src >> BLUE_6) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;

                if (SPG_CONTROL.interlace) {
                    dst += width * 4;
                }
            }
            break;
        case fbde_888: // 888 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    if (addr & 1)
                    {
                        u32 src = pvr_read_area1_32(vram, addr - 1);
                        *dst++ = src >> RED_8;
                        *dst++ = src >> 8;
                        *dst++ = src >> BLUE_8;
                    }
                    else
                    {
                        u32 src = pvr_read_area1_32(vram, addr);
                        *dst++ = src >> (RED_8 + 8);
                        *dst++ = src >> 16;
                        *dst++ = src >> (BLUE_8 + 8);
                    }
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;

                if (SPG_CONTROL.interlace) {
                    dst += width * 4;
                }
            }
            break;
        case fbde_C888: // 0888 RGB
            for (int y = 0; y < height; y++)
            {
                for (int i = 0; i < width; i++)
                {
                    u32 src = pvr_read_area1_32(vram, addr);
                    *dst++ = src >> RED_8;
                    *dst++ = src >> 8;
                    *dst++ = src >> BLUE_8;
                    *dst++ = 0xFF;
                    addr += bpp;
                }
                addr += modulus * bpp;
                
                if (SPG_CONTROL.interlace) {
                    dst += width * 4;
                }
            }
            break;
        }
        
#if defined(REFSW_OFFLINE)
    char sname[256];
	sprintf(sname, "FB_W_SOF1.png");
	FILE *fp = fopen(sname, "wb");
    if (fp == NULL)
    	return;

    auto CurrentRow = (png_bytep)pb.data();

    if (SPG_CONTROL.interlace & SPG_STATUS.fieldnum) {
        CurrentRow += width * 4;
    }

	png_bytepp rows = (png_bytepp)malloc(height * sizeof(png_bytep));
	for (int y = 0; y < height; y++) {
		rows[y] = CurrentRow;//(png_bytep)malloc(w * 4);	// 32-bit per pixel
		//glReadPixels(0, y, w, 1, GL_RGBA, GL_UNSIGNED_BYTE, rows[y]);
        CurrentRow += width * 4;
        if (SPG_CONTROL.interlace) {
            CurrentRow += width * 4;
        }
	}

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    png_init_io(png_ptr, fp);


    /* write header */
    png_set_IHDR(png_ptr, info_ptr, width, height,
                         8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                         PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);


            /* write bytes */
    png_write_image(png_ptr, rows);

    /* end write */
    png_write_end(png_ptr, NULL);
    fclose(fp);

/*
	for (int y = 0; y < h; y++)
		free(rows[y]);*/
	free(rows);
#elif HOST_OS == OS_WINDOWS
        u32 *psrc = pb.data();
        SetDIBits(hmem, hBMP, 0, SPG_CONTROL.interlace ? height * 2 : height, psrc, (BITMAPINFO*)& bi, DIB_RGB_COLORS);

        RECT clientRect;

        GetClientRect(hWnd, &clientRect);

        HDC hdc = GetDC(hWnd);
        int w = clientRect.right - clientRect.left;
        int h = clientRect.bottom - clientRect.top;
        int x = (w - 640) / 2;
        int y = (h - 480) / 2;

        StretchBlt(hdc, x, y+480, 640, -480, hmem, 0, 0, 640, 480, SRCCOPY);
        ReleaseDC(hWnd, hdc);
#elif defined(SUPPORT_X11)
        extern Window x11_win;
        extern Display* x11_disp;
        extern Visual* x11_vis;

        extern int x11_width;
        extern int x11_height;
        
        u32 *psrc = pb.data();
        XImage* ximage = XCreateImage(x11_disp, x11_vis, 24, ZPixmap, 0, (char*)psrc, width, SPG_CONTROL.interlace ? height * 2 : height, 32, width * 4);

        GC gc = XCreateGC(x11_disp, x11_win, 0, 0);
        XPutImage(x11_disp, x11_win, gc, ximage, 0, 0, (x11_width - width) / 2, (x11_height - (SPG_CONTROL.interlace ? height * 2 : height)) / 2, width, SPG_CONTROL.interlace ? height * 2 : height);
        XFree(ximage);
        XFreeGC(x11_disp, gc);
#else
        // TODO softrend without X11 (SDL f.e.)
    die("Softrend doesn't know how to update the screen");
#endif
    }
};

Renderer* rend_refred_base(u8* vram, function<RefRendInterface*()> createBackend) {
    return new refrend(vram, createBackend);
}
