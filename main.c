#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <x86intrin.h>   
#include <emmintrin.h>      
#include <xmmintrin.h>      
#include <sys/utsname.h>
#include <unistd.h>
#include <math.h>        
#include "aes.h"

#define M                    50000000LL     
#define WARMUP               500000         
#define BATCH_SIZE           10000      
#define OUTLIER_CAP          15000ULL   
#define EVICTION_SIZE        32768      

int main(void) {
    printf("Micro-architecture Security – Assignment 1\n");
    printf("Profiling phase of Bernstein cache-timing attack\n\n");

    struct utsname u;
    uname(&u);
    printf("Kernel version:          %s\n", u.release);

    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    char line[256];
    if (cpuinfo) {
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0) {
                printf("CPU model:               %s", line);
                break;
            }
        }
        fclose(cpuinfo);
    }

    printf("Ubuntu version:          22.04.5 LTS (jammy)\n");
    printf("Compiler:                gcc %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    printf("AES impl:                Custom T-table AES-128 (aes.c / aes.h)\n");
    printf("Fixed key K:             all zeros\n");
    printf("Total encryptions M:     %lld\n\n", M);

    uint8_t key[16] = {0};
    uint32_t expanded_key[Nb * (Nr + 1)];
    aes_key_expand(key, expanded_key);

    unsigned long long T[16][256] = {{0}};
    unsigned long long C[16][256] = {{0}};

    srand(time(NULL));

    printf("Pre-generating %d random plaintexts...\n", BATCH_SIZE);
    uint8_t *plain_batch = malloc(BATCH_SIZE * 16);
    if (!plain_batch) {
        perror("malloc plaintext batch");
        return 1;
    }
    for (long long b = 0; b < BATCH_SIZE; b++) {
        for (int k = 0; k < 16; k++) {
            plain_batch[b * 16 + k] = (uint8_t)(rand() % 256);
        }
    }

    printf("Warming up cache (%d encryptions)...\n", WARMUP);
    for (long long w = 0; w < WARMUP; w++) {
        uint8_t in[16], out[16];
        for (int k = 0; k < 16; k++) in[k] = rand() % 256;
        aes_encrypt(in, out, expanded_key);
    }

    printf("Starting profiling (%lld encryptions)...\n", M);

    long long batch_idx = 0;
    for (long long m = 0; m < M; m++) {
        uint8_t *plaintext = &plain_batch[batch_idx * 16];
        batch_idx = (batch_idx + 1) % BATCH_SIZE;

        uint8_t ciphertext[16];

        volatile uint8_t eviction_set[EVICTION_SIZE];
        for (int k = 0; k < EVICTION_SIZE; k += 64) {
            eviction_set[k] = (uint8_t)(k ^ (m & 0xFF));
        }

        _mm_mfence(); _mm_lfence();
        unsigned long long start = __rdtsc();
        
        _mm_lfence();

        aes_encrypt(plaintext, ciphertext, expanded_key);

        _mm_lfence();
        unsigned long long end = __rdtsc();
        _mm_lfence();

        unsigned long long tau = end - start;

        for (int k = 0; k < EVICTION_SIZE; k += 64) {
            eviction_set[k] ^= (uint8_t)m;
        }

        if (tau > OUTLIER_CAP) tau = OUTLIER_CAP;

        for (int i = 0; i < 16; i++) {
            int j = plaintext[i];
            T[i][j] += tau;
            C[i][j]++;
        }

        if (m % 5000000LL == 0 && m > 0) {
            printf("  Progress: %lld / %lld  (%.1f%%)\n", m, M, 100.0 * m / M);
        }
    }

    free(plain_batch);

    double mu[16][256];
    double mu_all[16];
    double D[16][256];

    for (int i = 0; i < 16; i++) {
        double sum_mu = 0.0;
        for (int j = 0; j < 256; j++) {
            mu[i][j] = (C[i][j] > 0) ? (double)T[i][j] / C[i][j] : 0.0;
            sum_mu += mu[i][j];
        }
        mu_all[i] = sum_mu / 256.0;

        for (int j = 0; j < 256; j++) {
            D[i][j] = mu[i][j] - mu_all[i];
        }
    }

    FILE *fp = fopen("D_profile_matrix.csv", "w");
    if (!fp) {
        perror("Cannot create D_profile_matrix.csv");
        return 1;
    }

    fprintf(fp, "position");
    for (int j = 0; j < 256; j++) fprintf(fp, ",%d", j);
    fprintf(fp, "\n");

    for (int i = 0; i < 16; i++) {
        fprintf(fp, "%d", i);
        for (int j = 0; j < 256; j++) {
            fprintf(fp, ",%.6f", D[i][j]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);

    double min_d = 1e9, max_d = -1e9, sum_abs = 0.0;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 256; j++) {
            double v = D[i][j];
            min_d = v < min_d ? v : min_d;
            max_d = v > max_d ? v : max_d;
            sum_abs += fabs(v);
        }
    }

    printf("\n=== Profiling Complete ===\n");
    printf("Deviation matrix D saved to: D_profile_matrix.csv\n");
    printf("Global statistics:\n");
    printf("  Min deviation     : %.3f cycles\n", min_d);
    printf("  Max deviation     : %.3f cycles\n", max_d);
    printf("  Average |D|       : %.3f cycles\n\n", sum_abs / (16.0 * 256.0));

    printf("Per-position average |D|:\n");
    for (int i = 0; i < 16; i++) {
        double pos_sum = 0.0;
        for (int j = 0; j < 256; j++) pos_sum += fabs(D[i][j]);
        printf("  Position %2d: %.3f cycles\n", i, pos_sum / 256.0);
    }

    return 0;
}