#define STRINGIFY(n) #n
#define TO_FILE_NAME(n) STRINGIFY(cases/n.inl)
#define TEST_FN_NAME(n) test_##n
#define TEST_FN(n) void TEST_FN_NAME(n)()
#define CASE(n) { #n, &TEST_FN_NAME(n) }