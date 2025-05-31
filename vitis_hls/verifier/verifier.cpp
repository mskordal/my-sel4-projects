#include "verifier.h"
#include "sha256.h"

#define MAX_FUNCS 128 ///< Change according to list of keys passed
#define TERMIN_OFFSET 0x3FFFC
#define KEY_WORDS 12
#define KEY_WORD_OFFSET 2


word_t key_to_byte_key(attkey_t key, byte_t byte_key[BYTE_KEY_SIZE]);
bool are_hash_equal(byte_t hash1[HASH_SIZE], byte_t hash2[HASH_SIZE]);
word_t pseudo_random(word_t seed, bool load);

word_t verifier_top(word_t func_data, byte_t sw_hash[HASH_SIZE], byte_t* bram)
{
#pragma HLS INTERFACE mode=s_axilite	bundle=BUS_A port=return
#pragma HLS INTERFACE mode=s_axilite	bundle=BUS_A port=sw_hash
#pragma HLS INTERFACE mode=s_axilite	bundle=BUS_A port=func_data
#pragma HLS INTERFACE mode=m_axi		bundle=BUS_B port=bram

	byte_t hash[HASH_SIZE];
	byte_t byte_key[BYTE_KEY_SIZE];
	word_t byte_key_len;
	sha256_ctx_t ctx;
	word_t *bram_word;
	word_t curr_func_idx;

	static word_t nonce;
	static word_t bram_idx = 0;
	static word_t funcs_num = 0;
	static bool lfsr_load = true;

	static attkey_t keys[MAX_FUNCS] = {0};
	static word_t func_ids[MAX_FUNCS] = {0};
	word_t* termin_word = (word_t*) (bram + TERMIN_OFFSET);

	if (lfsr_load) //first time called? generate nonce and read keys
	{
		ap_uint<32> *bram_i32 = (ap_uint<32>*)bram;

		while(bram_i32[bram_idx] != 0)
		{
			func_ids[funcs_num] = bram_i32[bram_idx];
			for (word_t bram_offset = KEY_WORD_OFFSET; bram_offset < KEY_WORDS + KEY_WORD_OFFSET; bram_offset++)
			{
				#pragma HLS unroll
				keys[funcs_num] |= ap_uint<384>(bram_i32[bram_idx + bram_offset]) << (bram_offset-2)*32;
				bram_i32[bram_idx + bram_offset] = 0;
			}
			funcs_num++;
			bram_idx += 16;
		}
		nonce = pseudo_random((word_t)(keys[0] & 0xFFFFFFFF), lfsr_load);
		lfsr_load = false;
		bram_idx = 0;

		// DEBUG WRITE
		// for(int i = 0; i < funcs_num; i++)
		// {
		// 	bram[i] = (byte_t)func_ids[i];
		// }
		// for(int i = funcs_num; i < funcs_num + 48; i++)
		// {
		// 	bram[i] = (byte_t)(((ap_uint<384>(0xff) << 8*(i-funcs_num)) & keys[0]) >> 8*(i-funcs_num));
		// }
		return nonce;
	}
	if(func_data != 0)
	{
		word_t func_idx = -1;
		for (int i = 0; i < funcs_num; i++)
		{
			if (func_ids[i] == func_data)
			{
				func_idx = i;
				break;
			}
		}

		// Otherwise do hash etc and generate the next nonce at the end.
		byte_key_len = key_to_byte_key(keys[func_idx], byte_key);
		byte_key_len = append_nonce_to_byte_key(nonce, byte_key_len, byte_key);

		sha256_init(&ctx);
		sha256_update(&ctx, byte_key, byte_key_len);
		sha256_final(&ctx, hash);

		bool hash_equal = are_hash_equal(sw_hash, hash);
		nonce = pseudo_random(0, 0);

		if(hash_equal)
			bram[bram_idx] = 4;
		else
			bram[bram_idx] = 2;

		// DEBUG WRITE
		// for(int i = bram_idx*32; i < bram_idx*32 + 32; i++)
		// {
		// 	bram[i] = hash[i];
		// }
		bram_idx++;
	}
	else
	{
		termin_word[0] = 0x1;
	}
	return nonce;
}

// int key_to_byte_key(ap_uint<MAX_KEY_BITS> key, ap_uint<BYTE> byte_key[BYTE_KEY_SIZE])
word_t key_to_byte_key(attkey_t key, byte_t byte_key[BYTE_KEY_SIZE])
{
	word_t byte_key_len;
	attkey_t byte_mask = 0xff;
	for (byte_key_len = 0; key > 0; byte_key_len++)
	{
		byte_key[byte_key_len] = (byte_t)(key & byte_mask);
		key >>= BYTE;
	}
	return byte_key_len;
}

word_t append_nonce_to_byte_key(word_t nonce, word_t byte_key_len, byte_t byte_key[BYTE_KEY_SIZE])
{
	word_t new_key_len;
	word_t byte_mask = 0xff;
	for(new_key_len = byte_key_len; nonce != 0; new_key_len++)
	{
		byte_key[new_key_len] = (byte_t)(nonce & byte_mask);
		nonce >>= BYTE;
	}
	return new_key_len;
}

bool are_hash_equal(byte_t hash1[HASH_SIZE], byte_t hash2[HASH_SIZE])
{
	for (int i=0; i<HASH_SIZE; i++)
	{
		if (hash1[i] != hash2[i])
		{
			return false;
		}
	}
	return true;
}

/**
 * Produces a pseudorandom number every time called. Use load=1, the first time
 * to take the seed. Then for every other call, the new pseudorandom is based
 * on the previous result, hence seed and load don't matter. 
 */
word_t pseudo_random(word_t seed, bool load) {
	static ap_uint<32> lfsr;
	if (load) lfsr = seed;
	bool b_32 = lfsr.get_bit(32-32);
	bool b_22 = lfsr.get_bit(32-22);
	bool b_2 = lfsr.get_bit(32-2);
	bool b_1 = lfsr.get_bit(32-1);
	bool new_bit = b_32 ^ b_22 ^ b_2 ^ b_1;
	lfsr = lfsr >> 1;
	lfsr.set_bit(31, new_bit);

	return lfsr.to_uint();

}