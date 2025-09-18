#include "data_src_decoder.h"

#include <assert.h>
#include <string.h>

struct test_data_src {
    uint64_t data_src;
    const char *expected_str;
};

int main() {
    /* 229080144 |OP STORE|LVL L1 hit|SNP N/A|TLB L1 hit|LCK N/A|BLK N/A */
    /* 229080142 |OP LOAD|LVL L1 hit|SNP N/A|TLB L1 hit|LCK N/A|BLK N/A */
    /* 1e05080021 |OP N/A|LVL N/A|SNP N/A|TLB N/A|LCK N/A|BLK N/A */
    /* 629800842 |OP LOAD|LVL L3 hit|SNP HitM|TLB L1 hit|LCK N/A|BLK N/A */
    uint64_t test_cases[] = {
        0x229080144ULL,   // STORE, L1 hit, TLB L1 hit
        0x229080142ULL,   // LOAD, L1 hit, TLB L1 hit
        0x1e05080021ULL,  // N/A case
        0x629800842ULL,    // LOAD, L3 hit, SNP HitM, TLB L1 hit
        0x1a49081042ULL, //|OP LOAD|LVL RAM hit|SNP N/A|TLB L2 hit|LCK N/A|BLK 
        0x1e29080024ULL //|OP STORE|LVL N/A|SNP N/A|TLB L1 hit|LCK N/A|BLK  N/A 
    };

    struct test_data_src tests[] = {
        {0x229080144ULL, "OP STORE|LVL L1 hit|SNP N/A|TLB L1 hit|LCK N/A|BLK N/A"},
        {0x229080142ULL, "OP LOAD|LVL L1 hit|SNP N/A|TLB L1 hit|LCK N/A|BLK N/A"},
        {0x1e05080021ULL, "OP N/A|LVL N/A|SNP N/A|TLB N/A|LCK N/A|BLK N/A"},
        {0x629800842ULL, "OP LOAD|LVL L3 hit|SNP HitM|TLB L1 hit|LCK N/A|BLK N/A"},
        {0x1a49081042ULL, "OP LOAD|LVL RAM hit|SNP N/A|TLB L2 hit|LCK N/A|BLK N/A"},
        {0x1e29080024ULL, "OP STORE|LVL N/A|SNP N/A|TLB L1 hit|LCK N/A|BLK N/A"}
  };

    for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        char buf[128];
        get_data_src_decode_str(test_cases[i], buf, sizeof(buf));
        printf("Test case %d: %s\n", i, buf);
        printf("Expected: %s\n", tests[i].expected_str);
        assert(strcmp(buf, tests[i].expected_str) == 0);
    }

    printf("All tests passed!\n");
    return 0;
}
