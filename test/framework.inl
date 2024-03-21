#define STRINGIFY(n) #n
#define TO_FILE_NAME(n) STRINGIFY(cases/n.inl)
#define TEST_FN_NAME(n) test_##n
#define TEST_FN(n) void TEST_FN_NAME(n)()
#define CASE(n) { #n, &TEST_FN_NAME(n) }

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

pvr32words_t to_words(pvr32addr_t vram32_ptr) {
    return vram32_ptr / 4;
}