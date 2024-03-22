#include "types.h"
#include "refsw_tile.h"
#include "pvr_mem.h"
#include "pvr_regs.h"

template <typename T>
void vram32_write(u32* addr, T entry) {
    static_assert(sizeof(entry) % 4 == 0);
    auto p = (u32*)&entry;
    for (size_t i = 0; i < sizeof(entry); i+=4) {
        *vrp(*addr) = *p;
        *addr+=4;
        p++;
    }
}

inline pvr32words_t to_words(pvr32addr_t vram32_ptr) {
    return vram32_ptr / 4;
}