PARAM_BASE = 0;
FPU_PARAM_CFG |= 1 << 21;
SCALER_CTL.vscalefactor = 0x400;
FB_W_CTRL.fb_packmode = 0x1;
FB_R_SIZE.fb_x_size = 640/2-1;
FB_R_SIZE.fb_y_size = 480-1;

u32 op_list_base = 0x1000;

u32 param_offset = 0x2000;

u32 tiles_ptr = PARAM_BASE;

region_array_v2(&tiles_ptr, {
    .control = { .last_region = 1 },
    .opaque = { .ptr_in_words = op_list_base/4 },
    .opaque_mod = { .empty = 1 },
    .trans = { .empty = 1 },
    .trans_mod = { .empty = 1 },
    .puncht = { .empty = 1 }
});

auto obj = (ObjectListEntry*)vrp(op_list_base);
*obj = { .full = 0 };
obj->tstrip = { .param_offs_in_words = param_offset/4, .mask = 1 };

obj = (ObjectListEntry*)vrp(op_list_base + 4);
*obj = { .full = 0 };
obj->type = 7;
obj->link.end_of_list = 1;

RenderPVR();
RenderFramebuffer();