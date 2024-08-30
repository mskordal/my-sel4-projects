#ifndef KEYGEN_H
#define KEYGEN_H

#include <stdint.h>

#define TOTAL_SUBKEYS 12
#define TOTAL_EVENTS 6
#define UINT32_WIDTH 32
#define UINT64_WIDTH 64

/**************************** DATA TYPES ****************************/
typedef struct
{
	uint32_t key[TOTAL_SUBKEYS];
	int subkey_idx;
	int used_subkey_bits;
}attkey_t;
/*********************** FUNCTION DECLARATIONS **********************/
void create_attkey(attkey_t *attkey, uint64_t *counters, int *event_shifts);
void print_bin(uint64_t n);
void print_attkey(attkey_t* key);

#endif   // KEYGEN_H
