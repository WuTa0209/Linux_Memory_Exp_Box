#ifndef DATA_SRC_DECODER_H
#define DATA_SRC_DECODER_H

#include <stdint.h>
#include <stdio.h>

void get_data_src_decode_str(uint64_t data_src, char *buf, size_t buf_size);
int is_cache_miss(uint64_t data_src, uint64_t cache_level);
int is_tlb_miss(uint64_t data_src);
void decode_data_src(uint64_t data_src);

#endif // DATA_SRC_DECODER_H