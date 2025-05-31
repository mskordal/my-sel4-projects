#include "../include/snipmath.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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

int main(void)
{
	double  a1 = 1.0, b1 = -10.5, c1 = 32.0, d1 = -30.0;
	double  a2 = 1.0, b2 = -4.5, c2 = 17.0, d2 = -30.0;
	double  a3 = 1.0, b3 = -3.5, c3 = 22.0, d3 = -31.0;
	double  a4 = 1.0, b4 = -13.7, c4 = 1.0, d4 = -35.0;
	double  x[3];
	double X;
	int     solutions;
	int i;
	unsigned long l = 0x3fed0169L;
	struct int_sqrt q;
	long n = 0;

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

	/* solve soem cubic functions */
	printf("********* CUBIC FUNCTIONS ***********\n");
	/* should get 3 solutions: 2, 6 & 2.5   */
	SolveCubic(a1, b1, c1, d1, &solutions, x);  
	printf("Solutions:");
	for(i=0;i<solutions;i++)
		printf(" %f",x[i]);
	printf("\n");
	/* should get 1 solution: 2.5           */
	SolveCubic(a2, b2, c2, d2, &solutions, x);  
	printf("Solutions:");
	for(i=0;i<solutions;i++)
		printf(" %f",x[i]);
	printf("\n");
	SolveCubic(a3, b3, c3, d3, &solutions, x);
	printf("Solutions:");
	for(i=0;i<solutions;i++)
		printf(" %f",x[i]);
	printf("\n");
	SolveCubic(a4, b4, c4, d4, &solutions, x);
	printf("Solutions:");
	for(i=0;i<solutions;i++)
		printf(" %f",x[i]);
	printf("\n");
	/* Now solve some random equations */
	/*for(a1=1;a1<10;a1++) {*/
	for(a1=1;a1<7;a1++) {
		/*for(b1=10;b1>0;b1--) {*/
		for(b1=7;b1>0;b1--) {
			/*for(c1=5;c1<15;c1+=0.5) {*/
			for(c1=5;c1<11;c1+=0.5) {
				/*for(d1=-1;d1>-11;d1--) {*/
				for(d1=-1;d1>-7;d1--) {
					SolveCubic(a1, b1, c1, d1, &solutions, x);  
					/*printf("Solutions:");*/
					/*for(i=0;i<solutions;i++)*/
						/*printf(" %f",x[i]);*/
					/*printf("\n");*/
				}
			}
		}
	}
	printf("Solved 18000 random equations\n\n");

	printf("********* INTEGER SQR ROOTS ***********\n");
	/* perform some integer square roots */
	/*for (i = 0; i < 1001; ++i)*/
	for (i = 0; i < 250; ++i)
	{
		usqrt(i, &q);
		// remainder differs on some machines
		// printf("sqrt(%3d) = %2d, remainder = %2d\n",
		/*printf("sqrt(%3d) = %2d\n",*/
				/*i, q.sqrt);*/
	}
	usqrt(l, &q);
	//printf("\nsqrt(%lX) = %X, remainder = %X\n", l, q.sqrt, q.frac);
	/*printf("\nsqrt(%lX) = %X\n", l, q.sqrt);*/
	printf("Solved 18000 q.sqrt\n\n");


	printf("********* ANGLE CONVERSION ***********\n");
	/* convert some rads to degrees */
	for (X = 0.0; X <= 360.0; X += 1.0)
		deg2rad(X);
		/*printf("%3.0f degrees = %.12f radians\n", X, deg2rad(X));*/
	printf("Solved 361 deg2rad\n");
	/*puts("");*/
	for (X = 0.0; X <= (2 * PI + 1e-6); X += (PI / 180))
		rad2deg(X);
		/*printf("%.12f radians = %3.0f degrees\n", X, rad2deg(X));*/
	printf("Solved 361 rad2deg\n\n");


	return 0;
}
