/* KallistiOS ##version##

   hello.c
   Copyright (C) 2001 Megan Potter
*/

#include <kos.h>

/* You can safely remove this line if you don't use a ROMDISK */
extern uint8 romdisk[];

/* These macros tell KOS how to initialize itself. All of this initialization
   happens before main() gets called, and the shutdown happens afterwards. So
   you need to set any flags you want here. Here are some possibilities:

   INIT_NONE        -- don't do any auto init
   INIT_IRQ         -- Enable IRQs
   INIT_NET         -- Enable networking (including sockets)
   INIT_MALLOCSTATS -- Enable a call to malloc_stats() right before shutdown

   You can OR any or all of those together. If you want to start out with
   the current KOS defaults, use INIT_DEFAULT (or leave it out entirely). */
KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);

/* And specify a romdisk, if you want one (or leave it out) */
KOS_INIT_ROMDISK(romdisk);

#define PVR_REG_BASE 0xA05f8000

#define SOFT_RESET 0x8
#define START 0x14
#define FB_W_SOF1 0x60

#define PVR_REG(num) (*(volatile uint32_t*)(PVR_REG_BASE + num))

#define PVR_MEM_32_BASE ((volatile void*)0xA5000000)
#define PVR_MEM_SIZE (8 * 1024 * 1024)

#define INTERRUPT_REG (*(volatile uint32_t*)0xA05F6900)
#define VIDEO_DONE 1

/* Your program's main entry point */
int main(int argc, char **argv) {
    /* The requisite line */
    printf("\nta dump renderer!\n\n");

    printf("pvr reset ...\n");
    // Soft reset
    PVR_REG(SOFT_RESET) = 0x3; // ta & core
    // wait a bit here somehow
    PVR_REG(SOFT_RESET) = 0x0; // reset none

    printf("pvr reg load ...\n");
    {
        FILE* regs = fopen("/rd/pvr_regs.bin", "rb");
        if (!regs) {
            printf("failed to open /rd/pvr_regs.bin\n");
            return -1;
        }
        for (int i = 0; i < 2048; i++) {
            uint32_t reg_val;
            uint32_t reg_num = i * 4;

            fread(&reg_val, sizeof(reg_val), 1, regs);
            
            if (reg_num != SOFT_RESET && reg_num != START) {
                PVR_REG(reg_num) = reg_val;
            }
        }
        // set all except render start and software reset
        fclose(regs);
    }

    printf("pvr mem load ...\n");
    {
        FILE* mem = fopen("/rd/pvr_mem.bin", "rb");
        if (!mem) {
            printf("failed to open /rd/pvr_mem.bin\n");
            return -1;
        }
        fread((void*)PVR_MEM_32_BASE, PVR_MEM_SIZE, 1, mem);
        fclose(mem);
    }

    printf("Clearing video done ...\n");
    // Make sure done bit is cleared
    INTERRUPT_REG |= VIDEO_DONE;

    printf("Starting render ...\n");
    // Start render
    PVR_REG(START) = 1;

    // wait for render to complete

    while (! INTERRUPT_REG & VIDEO_DONE) {
        // ... do nothing ....
    }

    printf("Render Finished ...\n");

    printf("TODO: Dump data better here\n");

    /*
        height = FB_R_SIZE.fb_y_size + 1;
        modulus = (FB_R_SIZE.fb_modulus - 1) << 1;
        color mode in FB_R_CTRL.fb_depth
        FB_W_SOF1 is where things were written to
    */

    volatile uint16_t *FB_W = PVR_REG(FB_W_SOF1) | 0xA5000000;

    printf("//%x\nuint16_t fb_data[]={\n", FB_W);
    for (int i = 0; i < (640 * 240); i++) {
        printf("%x,", FB_W[i]);
    }

    printf("};\n\nDone\n");
    return 0;
}