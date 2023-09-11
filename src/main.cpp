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
    if (argc != 3)
    {
        printf("expected %s <pvr_dump_folder> <dump name>\n", argv[0]);
        return -1;
    }

    path base_path(argv[1]);
    string dump_pvr_regs(argv[2]);
    string dump_vram(argv[2]);

    auto path_pvr_regs = base_path / ("pvr_regs" + dump_pvr_regs + ".bin");
    auto path_vram = base_path / ("vram" + dump_vram + ".bin");

    printf("loading vram from '%s'... likely to not work for 16mb vram ...\n", path_vram.c_str());
    {
        FILE* v0 = fopen(path_pvr_regs.c_str()).c_str(), "rb");
        for (size_t i = 0; i < VRAM_SIZE; i+= 4)
        {
            auto v = vrp(vram, i);
            fread(v, sizeof(*v), 1, v0);
        }
        fclose(v0);
    }
    
    printf("loading pvr regs from '%s'... likely to not work for 16mb vram ...\n", path_vram.c_str());
    {
        FILE* regs = fopen(path_pvr_regs.c_str()), "rb");
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