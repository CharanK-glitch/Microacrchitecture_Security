#include <sys/mman.h>
#include <x86intrin.h>
#include <vector>
#include <algorithm>
#include <random>
#include <iostream>
#include <cstdint>

/* ===== CPU PARAMETERS (YOUR MACHINE) ===== */
constexpr size_t PAGE_SIZE = 1UL << 30;   // ~1 GB (assumed)
constexpr size_t STRIDE    = 1UL << 16;   // 64 KB
constexpr int    LLC_WAYS  = 12;           // from lscpu -C
constexpr int    ROUNDS    = 50;

/* ===== Measure memory access latency ===== */
inline uint32_t access_time(volatile uint64_t* addr) {
    unsigned aux;
    uint64_t start = __rdtscp(&aux);
    (void)*addr;
    uint64_t end   = __rdtscp(&aux);
    return (uint32_t)(end - start);
}

/* ===== Check if a pool evicts target ===== */
bool evicts(const std::vector<uint64_t*>& pool,
            volatile uint64_t* target,
            uint32_t threshold) {

    (void)*target;
    _mm_lfence();

    for (int r = 0; r < ROUNDS; r++)
        for (auto p : pool)
            (void)*p;

    return access_time(target) > threshold;
}

/* ===== Build eviction set ===== */
void build_eviction(uint64_t* base) {

    /* Candidate addresses using 64 KB stride */
    std::vector<uint64_t*> candidates;
    for (size_t i = 1; i < 2000; i++)
        candidates.push_back(base + (i * STRIDE / sizeof(uint64_t)));

    std::shuffle(candidates.begin(), candidates.end(),
                 std::mt19937{std::random_device{}()});

    volatile uint64_t* target = base;

    /* Simple threshold calibration */
    uint64_t hit = 0, miss = 0;
    for (int i = 0; i < 500; i++) hit += access_time(target);
    _mm_clflush((void*)target);
    for (int i = 0; i < 500; i++) miss += access_time(target);

    uint32_t threshold = ((hit + miss) / 1000);

    std::cout << "[threshold] " << threshold << "\n";

    if (!evicts(candidates, target, threshold)) {
        std::cout << "Initial pool does not evict — abort\n";
        return;
    }

    /* Reduction phase */
    std::vector<uint64_t*> eviction_set;

    for (size_t i = 0; i < candidates.size();) {
        uint64_t* removed = candidates[i];
        candidates[i] = candidates.back();
        candidates.pop_back();

        if (!evicts(candidates, target, threshold))
            eviction_set.push_back(removed);
        else
            i++;

        if ((int)eviction_set.size() == LLC_WAYS)
            break;
    }

    /* Print eviction set */
    std::cout << "\nEviction set (" << eviction_set.size() << " lines):\n";
    for (auto p : eviction_set)
        std::cout << p << "\n";
}

int main() {

    std::cout << "Mapping ~1 GB region (assumed contiguous)\n";

    void* region = mmap(nullptr, PAGE_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);

    if (region == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    build_eviction((uint64_t*)region);

    munmap(region, PAGE_SIZE);
    return 0;
}