#ifndef VERIFIER_H
#define VERIFIER_H

#include <ap_int.h>
#include "sha256.h"
// #include "hls_stream.h"

#define BYTE 8
#define BYTE_KEY_SIZE 64

#define MAX_KEY_BITS MAX_COUNTER_BITS*EVENTS_NUM
#define MAX_KEY_BYTES MAX_KEY_BITS/BYTE

typedef ap_uint<384> attkey_t; // 6 counters * 64 bits each

word_t verifier_top(word_t func_data, byte_t sw_hash[HASH_SIZE], byte_t* bram);
word_t key_to_byte_key(attkey_t key, byte_t byte_key[BYTE_KEY_SIZE]);
word_t append_nonce_to_byte_key(word_t nonce, word_t byte_key_len, byte_t byte_key[BYTE_KEY_SIZE]);


#endif