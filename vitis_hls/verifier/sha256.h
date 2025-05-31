/*********************************************************************
* Filename:   sha256.h
* Author:     Brad Conte (brad AT bradconte.com)
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    Defines the API for the corresponding SHA1 implementation.
*********************************************************************/

#ifndef SHA256_H
#define SHA256_H

/*************************** HEADER FILES ***************************/
#include <ap_int.h>

/****************************** MACROS ******************************/
#define HASH_SIZE 32            // SHA256 outputs a 32 byte digest

/**************************** DATA TYPES ****************************/
typedef unsigned char byte_t;             // 8-bit byte
typedef unsigned int  word_t;          // 32-bit word_t, set to long for 16-bit

typedef struct {
	byte_t data[64];
	word_t datalen;
	// word_t bitlen;
	unsigned long long bitlen;
	word_t state[8];
} sha256_ctx_t;

/*********************** FUNCTION DECLARATIONS **********************/
void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const byte_t data[], unsigned long long len);
void sha256_final(sha256_ctx_t *ctx, byte_t hash[]);
#endif   // SHA256_H
