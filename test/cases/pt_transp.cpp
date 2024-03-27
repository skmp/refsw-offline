#include "test/framework.h"

void testCase() {
    PARAM_BASE = 0;
    FPU_PARAM_CFG |= 1 << 21;
    SCALER_CTL.vscalefactor = 0x400;
    FB_W_CTRL.fb_packmode = 0x1;
    FB_R_SIZE = { .fb_x_size = 32/2-1, .fb_y_size = 32-1, .fb_modulus = 1 };
    FB_R_CTRL = { .fb_depth = fbde_565 };

    FB_W_SOF1 = 4 * 1024 * 1024;
    FB_W_LINESTRIDE =  { .stride = 32 * 2 / 8 };

    PT_ALPHA_REF = 128;

    u32 op_list_base = 0x1000;

    u32 param_offset = 0x2000;


    auto param_ptr = PARAM_BASE + param_offset;

    ISP_BACKGND_T = { .tag_address_in_words = to_words(param_ptr - PARAM_BASE), .skip = 1 };
    ISP_BACKGND_D = {.f = 1.0f };

    vram32_write(&param_ptr, ISP_TSP {
        .Gouraud = 1,
        .CullMode = 0,
        .DepthMode = 7
    });

    vram32_write(&param_ptr, TSP {
        .DstInstr = 0,
        .SrcInstr = 1,
    });

    vram32_write(&param_ptr, TCW {});

    vram32_write(&param_ptr, 0.0f);
    vram32_write(&param_ptr, 0.0f);
    vram32_write(&param_ptr, 1.0f);
    vram32_write(&param_ptr, 0xFFFFFFFF);

    vram32_write(&param_ptr, 640.0f);
    vram32_write(&param_ptr, 0.0f);
    vram32_write(&param_ptr, 1.0f);
    vram32_write(&param_ptr, 0xFFFFFFFF);

    vram32_write(&param_ptr, 640.0f);
    vram32_write(&param_ptr, 480.0f);
    vram32_write(&param_ptr, 1.0f);
    vram32_write(&param_ptr, 0xFFFFFFFF);



    u32 tiles_ptr = PARAM_BASE;

    vram32_write(&tiles_ptr, RegionArrayEntry {
        .control = { .last_region = 1 },
        .opaque = { .empty = 1 },
        .opaque_mod = { .empty = 1 },
        .trans = { .empty = 1 },
        .trans_mod = { .empty = 1 },
        .puncht = { .ptr_in_words = to_words(op_list_base) }
    });

    vram32_write(&op_list_base, ObjectListTstrip {
        .param_offs_in_words = to_words(param_ptr - PARAM_BASE),
        .skip = 1,
        .mask = 1 << 5
    });

    vram32_write(&param_ptr, ISP_TSP {
        .Gouraud = 1,
        .CullMode = 0,
        .DepthMode = 7
    });

    vram32_write(&param_ptr, TSP {
        .UseAlpha = 1,
        .DstInstr = 0,
        .SrcInstr = 1,
    });

    vram32_write(&param_ptr, TCW {});

    vram32_write(&param_ptr, 6.0f);
    vram32_write(&param_ptr, 6.0f);
    vram32_write(&param_ptr, 1.0f);
    vram32_write(&param_ptr, 0xFF0000FF);

    vram32_write(&param_ptr, 28.0f);
    vram32_write(&param_ptr, 6.0f);
    vram32_write(&param_ptr, 1.0f);
    vram32_write(&param_ptr, 0xFF0000FF);

    vram32_write(&param_ptr, 28.0f);
    vram32_write(&param_ptr, 28.0f);
    vram32_write(&param_ptr, 1.0f);
    vram32_write(&param_ptr, 0xFF0000FF);

    vram32_write(&op_list_base, ObjectListTstrip {
        .param_offs_in_words = to_words(param_ptr - PARAM_BASE),
        .skip = 1,
        .mask = 1 << 5
    });

    vram32_write(&param_ptr, ISP_TSP {
        .Gouraud = 1,
        .CullMode = 0,
        .DepthMode = 7
    });

    vram32_write(&param_ptr, TSP {
        .UseAlpha = 1,
        .DstInstr = 0,
        .SrcInstr = 1,
    });

    vram32_write(&param_ptr, TCW {});


    vram32_write(&param_ptr, 8.0f);
    vram32_write(&param_ptr, 8.0f);
    vram32_write(&param_ptr, 2.0f);
    vram32_write(&param_ptr, 0x0000FF00);

    vram32_write(&param_ptr, 24.0f);
    vram32_write(&param_ptr, 8.0f);
    vram32_write(&param_ptr, 2.0f);
    vram32_write(&param_ptr, 0x0000FF00);

    vram32_write(&param_ptr, 24.0f);
    vram32_write(&param_ptr, 24.0f);
    vram32_write(&param_ptr, 2.0f);
    vram32_write(&param_ptr, 0x0000FF00);

    vram32_write(&op_list_base, ObjectListLink {
        .end_of_list = 1,
        .type = 7
    });

    RenderPVR();
    RenderFramebuffer();
}