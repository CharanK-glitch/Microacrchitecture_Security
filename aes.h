#ifndef AES_H
#define AES_H

#include <stdint.h>


#define Nk 4  
#define Nr 10 
#define Nb 4  


extern const uint8_t sbox[256];


extern const uint32_t Te0[256];
extern const uint32_t Te1[256];
extern const uint32_t Te2[256];
extern const uint32_t Te3[256];


extern const uint32_t Rcon[10];

void aes_key_expand(const uint8_t *key, uint32_t *expanded_key);
void aes_encrypt(const uint8_t *in, uint8_t *out, const uint32_t *expanded_key);

#endif 
