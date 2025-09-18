#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/perf_event.h>
/* Data source bit field definitions (based on perf_event.h) */

#define PERF_MEM_OP_SHIFT 0
#define PERF_MEM_LVL_SHIFT 5
#define PERF_MEM_SNP_SHIFT 19
#define PERF_MEM_TLB_SHIFT 26
#define PERF_MEM_LCK_SHIFT 32
#define PERF_MEM_BLK_SHIFT 40

/* Operation types */
#define PERF_MEM_OP_NA 0x01
#define PERF_MEM_OP_LOAD 0x02
#define PERF_MEM_OP_STORE 0x04
#define PERF_MEM_OP_PFETCH 0x08
#define PERF_MEM_OP_EXEC 0x10

/* Memory hierarchy levels */
#define PERF_MEM_LVL_NA 0x01
#define PERF_MEM_LVL_HIT 0x02
#define PERF_MEM_LVL_MISS 0x04
#define PERF_MEM_LVL_L1 0x08

/* Snoop types */
#define PERF_MEM_SNP_NA 0x01
#define PERF_MEM_SNP_NONE 0x02
#define PERF_MEM_SNP_HIT 0x04
#define PERF_MEM_SNP_MISS 0x08
#define PERF_MEM_SNP_HITM 0x10

/* TLB types */
#define PERF_MEM_TLB_NA 0x01
#define PERF_MEM_TLB_HIT 0x02
#define PERF_MEM_TLB_MISS 0x04
#define PERF_MEM_TLB_L1 0x08
#define PERF_MEM_TLB_L2 0x10
#define PERF_MEM_TLB_WK 0x20

/* Lock types */
#define PERF_MEM_LCK_NA 0x01
#define PERF_MEM_LCK_LOCKED 0x02

/* Block types */
#define PERF_MEM_BLK_NA 0x01
#define PERF_MEM_BLK_DATA 0x02
#define PERF_MEM_BLK_ADDR 0x04

/* Helper macros to extract fields */
#define PERF_MEM_OP(x) (((x) >> PERF_MEM_OP_SHIFT) & 0x1f)
#define PERF_MEM_LVL(x) (((x) >> PERF_MEM_LVL_SHIFT) & 0x3fff)
#define PERF_MEM_SNP(x) (((x) >> PERF_MEM_SNP_SHIFT) & 0x7f)
#define PERF_MEM_TLB(x) (((x) >> PERF_MEM_TLB_SHIFT) & 0x3f)
#define PERF_MEM_LCK(x) (((x) >> PERF_MEM_LCK_SHIFT) & 0xff)
#define PERF_MEM_BLK(x) (((x) >> PERF_MEM_BLK_SHIFT) & 0xff)

static inline int __is_cache_miss(uint64_t data_src, uint64_t cache_level) {
    uint64_t lvl = PERF_MEM_LVL(data_src);
    return (lvl & cache_level) && (lvl & PERF_MEM_LVL_MISS);
}

int is_cache_miss(uint64_t data_src, uint64_t cache_level) {
    return __is_cache_miss(data_src, cache_level);
}

static inline int __is_tlb_miss(uint64_t data_src) {
    uint64_t tlb = PERF_MEM_TLB(data_src);
    return tlb & PERF_MEM_TLB_MISS;
}

int is_tlb_miss(uint64_t data_src) {
    return __is_tlb_miss(data_src);
}

const char *decode_mem_op(uint64_t op) {
    if (op & PERF_MEM_OP_LOAD) return "LOAD";
    if (op & PERF_MEM_OP_STORE) return "STORE";
    if (op & PERF_MEM_OP_PFETCH) return "PFETCH";
    if (op & PERF_MEM_OP_EXEC) return "EXEC";
    return "N/A";
}

const char *decode_mem_lvl(uint64_t lvl) {
    static __thread char lvl_str[64];
    strcpy(lvl_str, "");

    if (lvl & PERF_MEM_LVL_NA) return "N/A";

    if (lvl & PERF_MEM_LVL_L1) strcat(lvl_str, "L1 ");
    if (lvl & PERF_MEM_LVL_L2) strcat(lvl_str, "L2 ");
    if (lvl & PERF_MEM_LVL_L3) strcat(lvl_str, "L3 ");
    if (lvl & PERF_MEM_LVL_LOC_RAM) strcat(lvl_str, "LOC_RAM ");
    if (lvl & PERF_MEM_LVL_REM_RAM1) strcat(lvl_str, "REM_RAM1 ");
    if (lvl & PERF_MEM_LVL_REM_RAM2) strcat(lvl_str, "REM_RAM2 ");

    if (lvl & PERF_MEM_LVL_HIT)
        strcat(lvl_str, "hit");
    else if (lvl & PERF_MEM_LVL_MISS)
        strcat(lvl_str, "miss");

    if (strlen(lvl_str) == 0) return "N/A";

    /* remove tail space */
    int len = strlen(lvl_str);
    if (len > 0 && lvl_str[len - 1] == ' ')
        lvl_str[len - 1] = '\0';

    return lvl_str;
}

const char *decode_mem_snp(uint64_t snp) {
    if (snp & PERF_MEM_SNP_HIT) return "Hit";
    if (snp & PERF_MEM_SNP_HITM) return "HitM";
    if (snp & PERF_MEM_SNP_MISS) return "Miss";
    if (snp & PERF_MEM_SNP_NONE) return "None";
    return "N/A";
}

const char *decode_mem_tlb(uint64_t tlb) {
    static __thread char tlb_str[64]; /* use thread-local storage */
    strcpy(tlb_str, "");

    if (tlb & PERF_MEM_TLB_NA) return "N/A";

    if (tlb & PERF_MEM_TLB_L1) strcat(tlb_str, "L1 ");
    if (tlb & PERF_MEM_TLB_L2) strcat(tlb_str, "L2 ");
    if (tlb & PERF_MEM_TLB_WK) strcat(tlb_str, "WK ");

    if (tlb & PERF_MEM_TLB_HIT)
        strcat(tlb_str, "hit");
    else if (tlb & PERF_MEM_TLB_MISS)
        strcat(tlb_str, "miss");

    if (strlen(tlb_str) == 0) return "N/A";

    /* remove tail space */
    int len = strlen(tlb_str);
    if (len > 0 && tlb_str[len - 1] == ' ')
        tlb_str[len - 1] = '\0';

    return tlb_str;
}

const char *decode_mem_lck(uint64_t lck) {
    if (lck & PERF_MEM_LCK_LOCKED) return "LOCKED";
    return "N/A";
}

const char *decode_mem_blk(uint64_t blk) {
    if (blk & PERF_MEM_BLK_DATA) return "DATA";
    if (blk & PERF_MEM_BLK_ADDR) return "ADDR";
    return "N/A";
}

void decode_data_src(uint64_t data_src) {
    uint64_t op = PERF_MEM_OP(data_src);
    uint64_t lvl = PERF_MEM_LVL(data_src);
    uint64_t snp = PERF_MEM_SNP(data_src);
    uint64_t tlb = PERF_MEM_TLB(data_src);
    uint64_t lck = PERF_MEM_LCK(data_src);
    uint64_t blk = PERF_MEM_BLK(data_src);

    printf("/* %llx |OP %s|LVL %s|SNP %s|TLB %s|LCK %s|BLK %s */\n",
           (unsigned long long)data_src,
           decode_mem_op(op),
           decode_mem_lvl(lvl),
           decode_mem_snp(snp),
           decode_mem_tlb(tlb),
           decode_mem_lck(lck),
           decode_mem_blk(blk));
}

void get_data_src_decode_str(uint64_t v, char *out, size_t out_sz) {
    int n = 0;
    /* get bit-mask */
    uint64_t op   = (v >> PERF_MEM_OP_SHIFT)    & 0x1FULL;   // 5 bits
    uint64_t lvl  = (v >> PERF_MEM_LVL_SHIFT)   & 0x3FFFULL; // 14 bits
    uint64_t snp  = (v >> PERF_MEM_SNOOP_SHIFT) & 0x1FULL;   // 5 bits
    uint64_t lck  = (v >> PERF_MEM_LOCK_SHIFT)  & 0x3ULL;    // 2 bits
    uint64_t tlb  = (v >> PERF_MEM_TLB_SHIFT)   & 0x7FULL;   // 7 bits

    /* OP */
    const char *op_str = "N/A";
    if (op & PERF_MEM_OP_LOAD)  op_str = "LOAD";
    else if (op & PERF_MEM_OP_STORE) op_str = "STORE";
    else if (op & PERF_MEM_OP_PFETCH) op_str = "PFETCH";
    else if (op & PERF_MEM_OP_EXEC)   op_str = "EXEC";
    n += snprintf(out + n, out_sz - n, "OP %s|", op_str);

    /* LVL（as perf source：level，miss/hit tag）
       EX：L1 hit / L3 miss / RAM hit */
    const char *lvl_str = "N/A";
    if (!(lvl & PERF_MEM_LVL_NA)) {
        if      (lvl & PERF_MEM_LVL_L1)       lvl_str = "L1";
        else if (lvl & PERF_MEM_LVL_L2)       lvl_str = "L2";
        else if (lvl & PERF_MEM_LVL_L3)       lvl_str = "L3";
        else if (lvl & PERF_MEM_LVL_LFB)      lvl_str = "LFB";
        else if (lvl & PERF_MEM_LVL_LOC_RAM)  lvl_str = "RAM";
        else if (lvl & PERF_MEM_LVL_REM_RAM1) lvl_str = "Remote RAM (1 hop)";
        else if (lvl & PERF_MEM_LVL_REM_RAM2) lvl_str = "Remote RAM (2 hops)";
        else if (lvl & PERF_MEM_LVL_REM_CCE1) lvl_str = "Remote Cache (1 hop)";
        else if (lvl & PERF_MEM_LVL_REM_CCE2) lvl_str = "Remote Cache (2 hops)";
        else if (lvl & PERF_MEM_LVL_IO)       lvl_str = "IO";
        else if (lvl & PERF_MEM_LVL_UNC)      lvl_str = "Uncached";
    }
    n += snprintf(out + n, out_sz - n, "LVL %s", lvl_str);
    if (!(lvl & PERF_MEM_LVL_NA)) {
        if (lvl & PERF_MEM_LVL_HIT)  n += snprintf(out + n, out_sz - n, " hit");
        if (lvl & PERF_MEM_LVL_MISS) n += snprintf(out + n, out_sz - n, " miss");
    }
    n += snprintf(out + n, out_sz - n, "|");

    /* SNP */
    const char *snp_str = "N/A";
    if (!(snp & PERF_MEM_SNOOP_NA)) {
        if      (snp & PERF_MEM_SNOOP_NONE) snp_str = "None";
        else if (snp & PERF_MEM_SNOOP_HIT)  snp_str = "Hit";
        else if (snp & PERF_MEM_SNOOP_MISS) snp_str = "Miss";
        else if (snp & PERF_MEM_SNOOP_HITM) snp_str = "HitM";
    }
    n += snprintf(out + n, out_sz - n, "SNP %s|", snp_str);

    /* TLB */
    const char *tlb_lvl = "N/A";
    if (!(tlb & PERF_MEM_TLB_NA)) {
        if      (tlb & PERF_MEM_TLB_L1) tlb_lvl = "L1";
        else if (tlb & PERF_MEM_TLB_L2) tlb_lvl = "L2";
        else if (tlb & PERF_MEM_TLB_WK) tlb_lvl = "HW walk";
        else if (tlb & PERF_MEM_TLB_OS) tlb_lvl = "OS fault";
    }
    n += snprintf(out + n, out_sz - n, "TLB %s", tlb_lvl);
    if (!(tlb & PERF_MEM_TLB_NA)) {
        if (tlb & PERF_MEM_TLB_HIT)  n += snprintf(out + n, out_sz - n, " hit");
        if (tlb & PERF_MEM_TLB_MISS) n += snprintf(out + n, out_sz - n, " miss");
    }
    n += snprintf(out + n, out_sz - n, "|");

    const char *lck_str = "N/A";
    if (!(lck & PERF_MEM_LOCK_NA)) {
        lck_str = (lck & PERF_MEM_LOCK_LOCKED) ? "LOCKED" : "No";
    }
    n += snprintf(out + n, out_sz - n, "LCK %s|", lck_str);

    /* BLK */
    n += snprintf(out + n, out_sz - n, "BLK N/A");
}