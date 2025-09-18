/*
 * ibs_reader.c  ——  Instruction-Based Sampling (IBS Op) reader
 *
 *   time_ns,pid,tid,cpu,ip,lin_addr,phys_addr,
 *   dc_miss,l2_miss,l3_miss,tlb_miss,data_src
 *
 *   gcc -O2 -Wall -pthread ibs_reader.c -o ibs_reader
 *   sudo ./ibs_reader
 *
 *  target:
 *  Output should be as same as the following command output:
 *      sudo perf record -d -e ibs_op// --phys-data -c 200000 -a -- sleep 10
 *      sudo perf script -F pid,tid,cpu,ip,addr,phys_addr,data_src
 *
 *      sudo perf record -d -e ibs_op/cnt_ctl=1,l3missonly=1/  --phys-data -c 200000 -a -- sleep 10 // can get RAM hit only 
 */
#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "data_src_decoder.h"

#define rmb() __sync_synchronize()
#define wmb() __sync_synchronize()
#define NEXT(type)               \
    ({                           \
        type __v = *(type *)raw; \
        raw += sizeof(type);     \
        __v;                     \
    })

#define SAMPLE_PERIOD 65535ULL
#define RING_PAGES 8    
#define SCRATCH_SZ 4096

static volatile int running = 1;
static void sigh(int sig) {
    (void)sig;
    running = 0;
}

/* monotonic ns */
static uint64_t mono_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1e9 + ts.tv_nsec;
}

/* Read ibs_op PMU type */
static int get_ibs_pmu_type(void) {
    FILE *f = fopen("/sys/bus/event_source/devices/ibs_op/type", "r");
    if (!f) {
        perror("open ibs_op pmu type");
        return -1;
    }
    int t;
    if (fscanf(f, "%d", &t) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return t;
}

struct cpu_ctx {
    int cpu;
    int fd;
    void *ring;
    FILE *csv;
};

static void *cpu_loop(void *arg) {
    struct cpu_ctx *c = arg;
    const size_t pg = sysconf(_SC_PAGESIZE);
    const size_t ring_sz = RING_PAGES * pg;
    const size_t mask = ring_sz - 1;
    struct perf_event_mmap_page *meta = c->ring;
    char *data = (char *)meta + pg;
    char scratch[SCRATCH_SZ];

    /* pin thread */
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(c->cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);

    uint64_t last_flush = mono_ns();

    while (running) {
        uint64_t head = meta->data_head;
        rmb();
        while (meta->data_tail != head) {
            uint64_t tail = meta->data_tail & mask;
            struct perf_event_header *h = (void *)(data + tail);

            /* if record cross ring tail, move to scratch */
            if (tail + h->size > ring_sz) {
                size_t first = ring_sz - tail;
                if (h->size > SCRATCH_SZ) {
                    fprintf(stderr, "record too large: %u\n", h->size);
                    running = 0;
                    break;
                }
                memcpy(scratch, data + tail, first);
                memcpy(scratch + first, data, h->size - first);
                h = (struct perf_event_header *)scratch;
            }

            if (h->type == PERF_RECORD_SAMPLE) {
                char *raw = (char *)(h + 1);

                /* according sample_type, parse the field */
                uint64_t ip = NEXT(uint64_t);        /* PERF_SAMPLE_IP */
                uint64_t pid_tid = NEXT(uint64_t);   /* PERF_SAMPLE_TID */
                uint64_t ts = NEXT(uint64_t);        /* PERF_SAMPLE_TIME */
                uint64_t lin_addr = NEXT(uint64_t);  /* PERF_SAMPLE_ADDR */
                uint64_t id = NEXT(uint64_t);        /* PERF_SAMPLE_ID */
                uint64_t cpu_res = NEXT(uint64_t);   /* PERF_SAMPLE_CPU */
                uint64_t data_src = NEXT(uint64_t);  /* PERF_SAMPLE_DATA_SRC */
                uint64_t phys_addr = NEXT(uint64_t); /* PERF_SAMPLE_PHYS_ADDR */

                (void)id; /* unused */

                uint32_t pid = pid_tid & 0xffffffff;
                uint32_t tid = pid_tid >> 32;
                uint32_t cpu = cpu_res & 0xffffffff;

                __attribute__((unused)) int dc_miss = is_cache_miss(data_src, PERF_MEM_LVL_L1);
                __attribute__((unused)) int l2_miss = is_cache_miss(data_src, PERF_MEM_LVL_L2);
                __attribute__((unused)) int l3_miss = is_cache_miss(data_src, PERF_MEM_LVL_L3);
                __attribute__((unused)) int tlb_miss = is_tlb_miss(data_src);

                phys_addr &= ((1ULL << 52) - 1);

                char decode_str[128];
                get_data_src_decode_str(data_src, decode_str, sizeof(decode_str));

                fprintf(c->csv,
                        "%" PRIu64
                        ",%u,%u,%u,0x%llx,0x%llx,0x%llx,0x%llx,%s\n",
                        ts, pid, tid, cpu,
                        (unsigned long long)ip,
                        (unsigned long long)lin_addr,
                        (unsigned long long)phys_addr,
                        (unsigned long long)data_src,
                        decode_str);

                /* For debugging, show data_src */
                if (getenv("DEBUG_DATASRC")) {
                    decode_data_src(data_src);
                }
            }
            meta->data_tail += h->size;
        }
        wmb();

        if (mono_ns() - last_flush > 1e9) {
            fflush(c->csv);
            last_flush = mono_ns();
        }
        usleep(3000);
    }
    return NULL;
}

int main(void) {
    signal(SIGINT, sigh);

    int pmu_type = get_ibs_pmu_type();
    if (pmu_type < 0) {
        fprintf(stderr, "ibs_op PMU not found\n");
        return 1;
    }

    int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    size_t pg = sysconf(_SC_PAGESIZE);

    pthread_t *th = calloc(ncpu, sizeof(pthread_t));
    struct cpu_ctx *ctx = calloc(ncpu, sizeof(struct cpu_ctx));

    FILE *csv = fopen("ibs_samples.csv", "w");
    if (!csv) {
        perror("fopen ibs_samples.csv");
        return 1;
    }

    fprintf(csv,
            "time_ns,pid,tid,cpu,ip,lin_addr,phys_addr,"
            "data_src,data_src_decoded\n");

    struct perf_event_attr attr = {0};
    attr.size = sizeof(attr);
    attr.type = pmu_type;
    attr.config = 0x90000; /* IBS Op event 0 */
    attr.sample_period = SAMPLE_PERIOD;
    attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_TIME |
                       PERF_SAMPLE_ADDR | PERF_SAMPLE_ID | PERF_SAMPLE_CPU |
                       PERF_SAMPLE_DATA_SRC | PERF_SAMPLE_PHYS_ADDR;
    attr.read_format = PERF_FORMAT_ID | PERF_FORMAT_LOST;
    attr.precise_ip = 2;
    attr.sample_id_all = 1;
    attr.disabled = 1;

    for (int cpu = 0; cpu < ncpu; ++cpu) {
        int fd =
            syscall(__NR_perf_event_open, &attr, -1, cpu, -1, PERF_FLAG_FD_CLOEXEC);
        if (fd < 0) {
            perror("perf_event_open");
            return 1;
        }

        size_t map_sz = (RING_PAGES + 1) * pg;
        void *ring = mmap(NULL, map_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ring == MAP_FAILED) {
            perror("mmap");
            return 1;
        }

        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

        ctx[cpu] = (struct cpu_ctx){.cpu = cpu, .fd = fd, .ring = ring, .csv = csv};
        pthread_create(&th[cpu], NULL, cpu_loop, &ctx[cpu]);
    }

    puts("IBS Op Collecting（Ctrl-C exit）…");
    puts("Setting DEBUG_DATASRC=1 can show data_src decode info. like sudo DEBUG_DATASRC=1 ./ibs_reader");

    while (running)
        pause();

    /* Wait for all threads to finish */
    for (int cpu = 0; cpu < ncpu; ++cpu) {
        pthread_join(th[cpu], NULL);
        close(ctx[cpu].fd);
    }

    fflush(csv);
    fclose(csv);
    puts("Finish，Write info into ibs_samples.csv");

    free(th);
    free(ctx);
    return 0;
}
