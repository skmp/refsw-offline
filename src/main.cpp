/*
    This was part of libswirl
*/
//#include <license/bsd>

#include <cstdio>
#include <stdarg.h>

#include <filesystem>

#include "Renderer_if.h"
#include "pvr_mem.h"
#include "pvr_regs.h"

using namespace std;
using namespace std::filesystem;

settings_t settings;

Renderer* rend_refsw(u8* vram);
Renderer* rend_refsw_debug(u8* vram);

u8 vram[VRAM_SIZE];

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("expected %s <pvr_dump_folder>\n", argv[0]);
        return -1;
    }

    path base_path(argv[1]);

    printf("loading dump from '%s'... likely to not work for 16mb vram ...\n", base_path.c_str());
    {
        FILE* v0 = fopen((base_path / "vram0.bin").c_str(), "rb");
        for (size_t i = 0; i < VRAM_SIZE/2; i+= 4)
        {
            auto v = vrp(vram, i);
            fread(v, sizeof(*v), 1, v0);
        }
        fclose(v0);
    }

    {
        FILE* v1 = fopen((base_path / "vram1.bin").c_str(), "rb");
        for (size_t i = VRAM_SIZE/2; i < VRAM_SIZE; i+= 4)
        {
            auto v = vrp(vram, i);
            fread(v, sizeof(*v), 1, v1);
        }
        fclose(v1);
    }
    
    {
        FILE* regs = fopen((base_path / "pvr_regs").c_str(), "rb");
        fread(pvr_regs, sizeof(pvr_regs), 1, regs);
        fclose(regs);
    }

    printf("Rendering ...\n");

    auto rend = rend_refsw(vram);

    rend->Init();

    rend->RenderPVR();

    printf("Hackpresenting ...\n");

    rend->RenderFramebuffer();

    return 0;
}

void os_DebugBreak()
{
	printf("DEBUGBREAK!\n");
    exit(-1);
}