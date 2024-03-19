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
        printf("expected %s path/vram_<name>.bin\n", argv[0]);
        return -1;
    }

    string dump_vram(argv[1]);

    auto path_vram = path(dump_vram);

    auto dump_pvr_regs = dump_vram.replace(dump_vram.find("vram_"), sizeof("vram_")-1, "pvr_regs_");

    auto path_pvr_regs = path(dump_pvr_regs);

    printf("loading vram from '%s'... likely to not work for 16mb vram ...\n", path_vram.c_str());
    {
		const char* fName = (const char*)path_vram.c_str();
        FILE* v0 = fopen(argv[1], "rb");
        if (!v0) {
            printf("Failed\n");
            return -1;
        }
        for (size_t i = 0; i < VRAM_SIZE; i+= 4)
        {
            auto v = vrp(vram, i);
            fread(v, sizeof(*v), 1, v0);
        }
        fclose(v0);
    }
    
    printf("loading pvr regs from '%s'...\n", path_pvr_regs.c_str());
    {
        FILE* regs = fopen(argv[2], "rb");
        if (!regs) {
            printf("Failed\n");
            return -1;
        }
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