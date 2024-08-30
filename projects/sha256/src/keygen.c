#include <stdio.h>
#include "keygen.h"

int get_active_bits(uint64_t n);
void store_counter_to_key(uint64_t counter, attkey_t *key);

/* Return the number of bits of n counted from the MS set to LS bit*/
int get_active_bits(uint64_t n)
{
	int msb_pos;
	if (n == 0)
		return 0;
	msb_pos = 1;
	while (n != 1)
	{
		msb_pos++;
		n = n >> 1;
	}
	return msb_pos;
}

void print_bin(uint64_t n)
{
	char bin[UINT64_WIDTH+1] = {0};
	int i;
	int bin_idx = UINT64_WIDTH - 2;

	while(n != 0)
	{
		for(i=0; i<4; i++)
		{
			bin[bin_idx--] = n%2 + '0';
			n >>= 1;
		}
	}
	printf("%s", &bin[bin_idx+1]);
}

void print_attkey(attkey_t* key)
{
	int i;
	int tmp_idx;

	tmp_idx = key->subkey_idx;
	printf("key(hex): ");
	printf("%x",  key->key[tmp_idx--]);
	while(tmp_idx >= 0)
	{
		printf("%x", key->key[tmp_idx--]);
	}
	printf("\n");
	/*tmp_idx = key->subkey_idx;*/
	/*printf("key(bin): ");*/
	/*print_bin(key->key[tmp_idx--]);*/
	/*while(tmp_idx >= 0)*/
	/*{*/
		/*print_bin(key->key[tmp_idx--]);*/
	/*}*/
	/*printf("\n");*/
}

void store_counter_to_key(uint64_t counter, attkey_t *key)
{
	uint32_t store_value;
	uint32_t lshift_store_value;
	uint32_t avail_bits_mask;;
	int counter_bits;
	int store_bits;
	int avail_subkey_bits;

	counter_bits = get_active_bits(counter);
	if(!counter_bits)
	{
		return;
	}

	avail_subkey_bits = UINT32_WIDTH - key->used_subkey_bits;
	if (counter_bits <= avail_subkey_bits)
	{
		store_value = counter;
		store_bits = counter_bits;
	}
	else
	{
		avail_bits_mask = UINT64_MAX >> (UINT64_WIDTH - avail_subkey_bits);
		store_value = avail_bits_mask & counter;
		store_bits = avail_subkey_bits;
	}
	lshift_store_value = store_value << key->used_subkey_bits;
	key->key[key->subkey_idx] |= lshift_store_value;
	counter >>= store_bits;
	counter_bits -= store_bits;
	key->used_subkey_bits = (key->used_subkey_bits + store_bits) % UINT32_WIDTH;
	key->subkey_idx += (key->used_subkey_bits == 0);

	while (counter_bits >= UINT32_WIDTH)
	{
		store_value = UINT32_MAX & counter;
		key->key[key->subkey_idx] = store_value;
		counter >>= UINT32_WIDTH;
		counter_bits -= UINT32_WIDTH;
		key->used_subkey_bits = 0;
		key->subkey_idx++;
	}
	if (counter_bits > 0)
	{
		lshift_store_value = counter << key->used_subkey_bits;
		key->key[key->subkey_idx] |= lshift_store_value;
		key->used_subkey_bits += counter_bits;
	}
}


void create_attkey(attkey_t *attkey, uint64_t *counters, int *event_shifts)
{
	uint64_t stable_counter;
	int event;

	memset(attkey, 0, sizeof(attkey_t));
	for(event = 0; event < TOTAL_EVENTS; event++)
	{
		stable_counter = counters[event] >> event_shifts[event];
		/*printf("%d:%lx - ", event, stable_counter);*/
		store_counter_to_key(stable_counter, attkey);
	}
	/*printf("\n");*/
	/*print_attkey(attkey);*/
}
