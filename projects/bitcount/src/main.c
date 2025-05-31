/* +++Date last modified: 05-Jul-1997 */

/*
 **  BITCNTS.C - Test program for bit counting functions
 **
 **  public domain by Bob Stout & Auke Reitsma
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <float.h>
#include "../include/conio.h"
#include "../include/bitops.h"

#include <sched.h>

#include <sel4/sel4.h>
#include <sel4platsupport/bootinfo.h>
#include <utils/util.h>
#include <sel4/sel4_arch/mapping.h>

#include <simple/simple.h>
#include <simple-default/simple-default.h>

#include <vka/object.h>
#include <allocman/allocman.h>
#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <vspace/vspace.h>

#include <sel4utils/vspace.h>
#include <sel4utils/mapping.h>
#include <sel4utils/process.h>
#include <sel4utils/thread.h>
#include <sel4bench/sel4bench.h>
#include <sha256.h>

/* The printf's may be removed to isolate just the math calculations */
#define HLS_BASE_ADDR				0xa0000000
#define BRAM_BASE_ADDR				0xa0040000
#define HLS_FUNCTION_COUNTER_OFFSET	0x10
#define HLS_BRAM_ADDR_OFFSET		0x18

#define HLS_SIZE_BITS 	12 // 2^12 = 4 KBytes
#define BRAM_SIZE_BITS 	16 // 2^15 = 32 KBytes
#define HLS_VADDR		0x10000000 // this virtual address works
#define BRAM_VADDR		0x10001000
#define BRAM_PAGES_NUM	48 // BIT(BRAM_SIZE_BITS) / BIT(seL4_PageBits)
#define MAP_A_DEVICE	true

/* global environment variables */
seL4_BootInfo *info;
simple_t simple;
vka_t vka;
allocman_t *allocman;
vspace_t vspace;

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE (BIT(seL4_PageBits) * 10)
UNUSED static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE (BIT(seL4_PageBits) * 100)

/* static memory for virtual memory bootstrapping */
UNUSED static sel4utils_alloc_data_t data;


#define FUNCS  7
#define ITERATIONS 128

static int CDECL bit_shifter(long int x);

int main(void)
{
	clock_t start, stop;
	double ct, cmin = DBL_MAX, cmax = 0;
	int i, cminix, cmaxix;
	long j, n, seed;
	int iterations;


	seL4_Error error = 0;
	info = platsupport_get_bootinfo();
	ZF_LOGF_IF(!info, "Failed to get bootinfo.");

	zf_log_set_tag_prefix("mainProcess:");
	NAME_THREAD(seL4_CapInitThreadTCB, "mainProcess");

	simple_default_init_bootinfo(&simple, info);
	allocman = bootstrap_use_current_simple(&simple, ALLOCATOR_STATIC_POOL_SIZE,
			allocator_mem_pool);
	ZF_LOGF_IF(allocman == NULL, "Failed to initialize allocator.\n");
	allocman_make_vka(&vka, allocman);
	seL4_CPtr pd_cap;
	pd_cap = simple_get_pd(&simple);
	error = sel4utils_bootstrap_vspace_with_bootinfo_leaky(&vspace,
			&data, pd_cap, &vka, info);
	ZF_LOGF_IFERR(error, "Failed to prepare root thread's VSpace for use.\n");

	unsigned long bram_paddr = BRAM_BASE_ADDR;
	unsigned long bram_vaddr = BRAM_VADDR;
	vka_object_t bram_frame_object[BRAM_PAGES_NUM];

	for(int i = 0; i < BRAM_PAGES_NUM; i++ )
	{
		error = vka_alloc_object_at_maybe_dev(&vka, seL4_ARM_SmallPageObject,
				seL4_PageBits, bram_paddr, MAP_A_DEVICE, &bram_frame_object[i]);
		ZF_LOGF_IFERR(error, "Failed to alloc a frame in BRAM.\n");

		vka_object_t bram_object;
		error = sel4utils_map_page(	&vka, pd_cap, bram_frame_object[i].cptr,
				(void*)bram_vaddr, seL4_ReadWrite, 0, NULL, NULL);
		ZF_LOGF_IFERR(error, "Failed to map bram frame to VSpace.\n");
		bram_paddr += BIT(seL4_PageBits);
		bram_vaddr += BIT(seL4_PageBits);
	}
	/*int *test = (int*)BRAM_VADDR;*/
	/*for(int i = 0; i < BRAM_PAGES_NUM; i++ )*/
	/*{*/
	/*printf("Will write to page with vaddr %p\n", &test[i*1024]);*/
	/*test[i*1024] = 1;*/
	/*}*/
	/*printf("Wrote all pages");*/

	vka_object_t hls_frame_object;

	vka_alloc_object_at_maybe_dev(	&vka, seL4_ARM_SmallPageObject,
			HLS_SIZE_BITS, HLS_BASE_ADDR, MAP_A_DEVICE, &hls_frame_object);
	ZF_LOGF_IFERR(error, "Failed to alloc a frame in HLS.\n");

	unsigned long hls_vaddr = HLS_VADDR;
	vka_object_t hls_objects[2];

	error = sel4utils_map_page(	&vka, pd_cap, hls_frame_object.cptr,
			(void*)hls_vaddr, seL4_ReadWrite, 0, hls_objects, NULL);
	ZF_LOGF_IFERR(error, "Failed to map hls frame to VSpace.\n");
	printf("Application is using pass!!\n");

	static int (* CDECL pBitCntFunc[FUNCS])(long) = {
		bit_count,
		bitcount,
		ntbl_bitcnt,
		ntbl_bitcount,
		/*            btbl_bitcnt, DOESNT WORK*/
		BW_btbl_bitcount,
		AR_btbl_bitcount,
		bit_shifter
	};
	static char *text[FUNCS] = {
		"Optimized 1 bit/loop counter",
		"Ratko's mystery algorithm",
		"Recursive bit count by nybbles",
		"Non-recursive bit count by nybbles",
		/*            "Recursive bit count by bytes",*/
		"Non-recursive bit count by bytes (BW)",
		"Non-recursive bit count by bytes (AR)",
		"Shift and count bits"
	};
	iterations = ITERATIONS;

	puts("Bit counter algorithm benchmark\n");

	for (i = 0; i < FUNCS; i++) {
		start = 0;

		for (j = n = 0, seed = rand(); j < iterations; j++, seed += 13)
			/*for (j = n = 0, seed = 49028394; j < iterations; j++, seed += 13)*/
			n += pBitCntFunc[i](seed);

		stop = 1;
		ct = (stop - start) / (double)CLOCKS_PER_SEC;
		if (ct < cmin) {
			cmin = ct;
			cminix = i;
		}
		if (ct > cmax) {
			cmax = ct;
			cmaxix = i;
		}

		printf("%-38s> Time: %7.3f sec.; Bits: %ld\n", text[i], ct, n);
	}
	printf("\nBest  > %s\n", text[cminix]);
	printf("Worst > %s\n", text[cmaxix]);
	return 0;
}

static int CDECL bit_shifter(long int x)
{
	int i, n;

	for (i = n = 0; x && (i < (sizeof(long) * CHAR_BIT)); ++i, x >>= 1)
		n += (int)(x & 1L);
	return n;
}
