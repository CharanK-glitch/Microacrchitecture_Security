#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <x86intrin.h>
#include <emmintrin.h>      // _mm_lfence
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>         // for size_t

#define CACHE_LINE_SIZE     64
#define LLC_WAYS            12
#define NUM_TRIALS          200     // More trials = better reliability
#define THRESHOLD_HIGH      350     // Tune after calibration (hit vs miss)
#define THRESHOLD_LOW       180     // Tune after calibration
#define POOL_SIZE           (1ULL << 31)  // 2GB total (adjust if needed)

static inline uint64_t rdtsc(void) {
    _mm_lfence();
    unsigned int dummy;
    uint64_t tsc = __rdtscp(&dummy);
    _mm_lfence();
    return tsc;
}

static inline void clflush(void *p) {
    asm volatile("clflush (%0)" :: "r"(p) : "memory");
}

static inline void prime(void **set, size_t len) {
    for (size_t i = 0; i < len; i++) {
        volatile uint64_t x = *(uint64_t *)set[i];
        (void)x;
    }
}

double probe(void **set, size_t set_len, void *target, void *candidate) {
    double total = 0.0;
    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        // Flush target to ensure it's not in cache
        clflush(target);

        // Prime the set
        prime(set, set_len);

        // Access candidate
        volatile uint64_t x = *(uint64_t *)candidate;
        (void)x;

        // Measure reload time of one address from set
        uint64_t start = rdtsc();
        volatile uint64_t y = *(uint64_t *)set[0];
        uint64_t end = rdtsc();
        (void)y;

        total += (double)(end - start);
    }
    return total / NUM_TRIALS;
}

void find_eviction_set(void *target) {
    // Step 1: Try to allocate 1GB huge pages
    void *pool = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB,
                      -1, 0);

    if (pool == MAP_FAILED) {
        perror("1GB huge pages failed");
        printf("Falling back to 2MB huge pages...\n");
        pool = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                    -1, 0);
        if (pool == MAP_FAILED) {
            perror("2MB huge pages also failed");
            printf("Falling back to standard 4KB pages...\n");
            pool = mmap(NULL, POOL_SIZE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (pool == MAP_FAILED) {
                perror("mmap failed");
                exit(1);
            }
        }
    }

    printf("Successfully allocated %zu bytes using huge pages.\n", POOL_SIZE);

    // Create array of cache-line-aligned addresses
    size_t num_addresses = POOL_SIZE / CACHE_LINE_SIZE;
    void **addresses = malloc(num_addresses * sizeof(void *));
    if (!addresses) {
        perror("malloc failed");
        exit(1);
    }

    for (size_t i = 0; i < num_addresses; i++) {
        addresses[i] = (void *)((uintptr_t)pool + i * CACHE_LINE_SIZE);
    }

    // Step 2: Build conflict set
    void **conflict_set = malloc(num_addresses * sizeof(void *));
    size_t conflict_len = 0;

    printf("Building conflict set...\n");
    for (size_t i = 0; i < num_addresses; i++) {
        double t = probe(addresses, num_addresses, target, addresses[i]);
        if (t > THRESHOLD_HIGH) {
            conflict_set[conflict_len++] = addresses[i];
            printf("Conflict found: %p (time %.2f)\n", addresses[i], t);
        }
    }

    printf("Conflict set size: %zu\n", conflict_len);

    if (conflict_len == 0) {
        printf("No conflicting addresses found. Try increasing POOL_SIZE or tuning thresholds.\n");
        goto cleanup;
    }

    // Step 3: Reduce to eviction set of size W
    void **eviction_set = malloc(LLC_WAYS * sizeof(void *));
    size_t evict_len = 0;

    printf("Reducing to eviction set of size %d...\n", LLC_WAYS);
    for (size_t i = 0; i < conflict_len && evict_len < LLC_WAYS; i++) {
        void *candidate = conflict_set[i];

        double t_with = probe(conflict_set, conflict_len, target, candidate);
        double t_without = probe(conflict_set, conflict_len - 1, target, candidate);

        if (t_with > THRESHOLD_HIGH && t_without < THRESHOLD_LOW) {
            eviction_set[evict_len++] = candidate;
            printf("Added to eviction set: %p (with=%.2f, without=%.2f)\n", candidate, t_with, t_without);
        }
    }

    printf("Eviction set size: %zu\n", evict_len);
    printf("Eviction set addresses:\n");
    for (size_t i = 0; i < evict_len; i++) {
        printf("  %zu: %p\n", i, eviction_set[i]);
    }

    // Validation (only if eviction set is non-empty)
    if (evict_len > 0) {
        double t_evicted = probe(eviction_set, evict_len, target, eviction_set[0]);
        printf("Validation - time with eviction set: %.2f cycles (should be high)\n", t_evicted);
    } else {
        printf("No eviction set found. Try increasing POOL_SIZE or tuning thresholds.\n");
    }

cleanup:
    free(addresses);
    free(conflict_set);
    free(eviction_set);
    munmap(pool, POOL_SIZE);
}

int main(void) {
    // Use a fixed target address (heap data)
    volatile uint8_t target_data[64] __attribute__((aligned(64)));
    void *target = (void *)target_data;

    printf("Target address: %p\n", target);

    find_eviction_set(target);

    return 0;
}