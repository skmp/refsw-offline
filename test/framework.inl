#define STRINGIFY(n) #n
#define TO_FILE_NAME(n) STRINGIFY(cases/n.inl)
#define TEST_FN_NAME(n) test_##n
#define TEST_FN(n) void TEST_FN_NAME(n)()
#define CASE(n) { #n, &TEST_FN_NAME(n) }

void region_array_v2(u32* addr, RegionArrayEntry entry) {
    *vrp(*addr) = entry.control.full; *addr += 4;
    *vrp(*addr) = entry.opaque.full; *addr += 4;
    *vrp(*addr) = entry.opaque_mod.full; *addr += 4;
    *vrp(*addr) = entry.trans.full; *addr += 4;
    *vrp(*addr) = entry.trans_mod.full; *addr += 4;
    *vrp(*addr) = entry.puncht.full; *addr += 4;
}