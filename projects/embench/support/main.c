#include <stdio.h>
#include <stdlib.h>
#include <float.h>
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

#include <sel4bench/sel4bench.h>

#include <attester_sw.h>

#include "support.h"

#define HLS_BASE_ADDR				0xa0000000
#define HLS_FUNCTION_COUNTER_OFFSET	0x10
#define HLS_BRAM_ADDR_OFFSET		0x18

#define BRAM_BASE_ADDR	0xa0010000
#define BRAM_SIZE_BITS 	16 // 2^16 = 8 KBytes
#define TEST_VADDR		0x7000000 // this virtual address works
#define MAP_A_DEVICE	true

#define APP_PRIORITY seL4_MaxPrio
#define APP_IMAGE_NAME "app"

#define ARRAY_SIZE_BITS 	19 // 2^19 = 256*256*8 Bytes = 512 KBytes
#define ARRAY_VADDR 0x6000000
#define MINd 0.0 // Min value in matrix
#define MAXd 1.0 // Max value in matrix
#define UNROLL 64 // Number of times to unroll loop in unrolled_matrix_multiply()
#define OMP_THREADS 12 // Number of threads to use with omp pragma
#define BLOCK_SIZE 32 // Block size when using blocked_matrix_multiply()
#define MEM_ALIGN 32 // Memory alignment required for _mm256 instructions
#define DIMENSION_SIZE 128

#define REPEAT 10000000  //9999999999

#define VARS unsigned long i,j;

#define CALC(x) for(i=0; i<REPEAT/x; i++) {++j;}


/* global environment variables */
seL4_BootInfo *info;
simple_t simple;
vka_t vka;
allocman_t *allocman;
vspace_t vspace;

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE (BIT(seL4_PageBits) * 100)
UNUSED static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE (BIT(seL4_PageBits) * 100)

/* static memory for virtual memory bootstrapping */
UNUSED static sel4utils_alloc_data_t data;

/* stack for the new thread */
#define THREAD_2_STACK_SIZE 4096
UNUSED static int thread_2_stack[THREAD_2_STACK_SIZE];

const char* benchName[benchmarksNum] =
{
	"aha-mont64",
	"crc32",
	"cubic",
	"edn",
	"huffbench",
	"matmult-int",
	"md5sum",
	"minver",
	"nbody",
	"nettle_aes",
	"nettle_sha256",
	"sichneu",
	"picojpeg",
	"primecount",
	"qrduino",
	"combined",
	"slre",
	"st",
	"statemate",
	"ud",
	"wikisort",
	"tarfind"
};

verify_f benchVerify[benchmarksNum] =
{
	&mont64_verify,
	&crc_32_verify,
	&basicmath_small_verify,
	&libedn_verify,
	&libhuffbench_verify,
	&matmult_int_verify,
	&md5_verify,
	&libminver_verify,
	&nbody_verify,
	&nettle_aes_verify,
	&nettle_sha256_verify,
	&libsichneu_verify,
	&libpicojpeg_test_verify,
	&primecount_verify,
	&qrtest_verify,
	&combined_verify,
	&libslre_verify,
	&libst_verify,
	&libstatemate_verify,
	&libud_verify,
	&libwikisort_verify,
	&tarfind_verify
};

init_f benchInit[benchmarksNum] =
{
	&mont64_init,
	&crc_32_init,
	&basicmath_small_init,
	&libedn_init,
	&libhuffbench_init,
	&matmult_int_init,
	&md5_init,
	&libminver_init,
	&nbody_init,
	&nettle_aes_init,
	&nettle_sha256_init,
	&libsichneu_init,
	&libpicojpeg_test_init,
	&primecount_init,
	&qrtest_init,
	&combined_init,
	&libslre_init,
	&libst_init,
	&libstatemate_init,
	&libud_init,
	&libwikisort_init,
	&tarfind_init
};

warm_f benchWarm[benchmarksNum] =
{
	&mont64_warm,
	&crc_32_warm,
	&basicmath_small_warm,
	&libedn_warm,
	&libhuffbench_warm,
	&matmult_int_warm,
	&md5_warm,
	&libminver_warm,
	&nbody_warm,
	&nettle_aes_warm,
	&nettle_sha256_warm,
	&libsichneu_warm,
	&libpicojpeg_test_warm,
	&primecount_warm,
	&qrtest_warm,
	&combined_warm,
	&libslre_warm,
	&libst_warm,
	&libstatemate_warm,
	&libud_warm,
	&libwikisort_warm,
	&tarfind_warm
};

run_f benchRun[benchmarksNum] =
{
	&mont64_run,
	&crc_32_run,
	&basicmath_small_run,
	&libedn_run,
	&libhuffbench_run,
	&matmult_int_run,
	&md5_run,
	&libminver_run,
	&nbody_run,
	&nettle_aes_run,
	&nettle_sha256_run,
	&libsichneu_run,
	&libpicojpeg_test_run,
	&primecount_run,
	&qrtest_run,
	&combined_run,
	&libslre_run,
	&libst_run,
	&libstatemate_run,
	&libud_run,
	&libwikisort_run,
	&tarfind_run
};

int main(void)
{
	seL4_Error error = 0;
	int i, result, correct;

	info = platsupport_get_bootinfo();
	ZF_LOGF_IF(!info, "Failed to get bootinfo.");

	/* Set up logging and give us a name: useful for debugging if the thread faults */
	zf_log_set_tag_prefix("mainProcess:");
	NAME_THREAD(seL4_CapInitThreadTCB, "mainProcess");

	/* init simple */
	simple_default_init_bootinfo(&simple, info);

	/* print out bootinfo and other info about simple */
	simple_print(&simple);

	/* create an allocator */
	allocman = bootstrap_use_current_simple(&simple, ALLOCATOR_STATIC_POOL_SIZE,
		allocator_mem_pool);
	ZF_LOGF_IF(allocman == NULL, "Failed to initialize allocator.\n");

	/* create a vka (interface for interacting with the underlying allocator) */
	allocman_make_vka(&vka, allocman);

	/* get our vspace root page directory */
	seL4_CPtr pd_cap;
	pd_cap = simple_get_pd(&simple);

	/* create a vspace object to manage the main thread's vspace */
	error = sel4utils_bootstrap_vspace_with_bootinfo_leaky(&vspace,
		&data, pd_cap, &vka, info);
	ZF_LOGF_IFERR(error, "Failed to prepare root thread's VSpace for use.\n");

	/* fill the allocator with virtual memory */
	void *vaddr;
	UNUSED reservation_t virtual_reservation;
	virtual_reservation = vspace_reserve_range(&vspace,
		ALLOCATOR_VIRTUAL_POOL_SIZE, seL4_AllRights, 1, &vaddr);
	ZF_LOGF_IF(virtual_reservation.res == NULL,
		"Failed to reserve a chunk of memory.\n");
	
	bootstrap_configure_virtual_pool(allocman, vaddr,
		ALLOCATOR_VIRTUAL_POOL_SIZE, simple_get_pd(&simple));


	/* use sel4utils to make a new process */
	sel4utils_process_t new_process;

	sel4utils_process_config_t config = process_config_default_simple(&simple,
		APP_IMAGE_NAME, APP_PRIORITY);
	error = sel4utils_configure_process_custom(&new_process, &vka, &vspace,
		config);
	ZF_LOGF_IFERR(error, "Failed to spawn a new thread.\n");

	/* give the new process's thread a name */
	NAME_THREAD(new_process.thread.tcb.cptr, "dynamic-3: process_2");

/**
	 * create arguments to pass to the process when it is spawned. Here we pass
	 * one argument of value 5
	 */
	seL4_Word argc = 1;
	char string_args[argc][WORD_STRING_SIZE];
	char* argv[argc];
	sel4utils_create_word_args(string_args, argv, argc, 5);

	/* spawn the process */
	error = sel4utils_spawn_process_v(&new_process, &vka, &vspace, argc,
		(char**) &argv, 1);
	ZF_LOGF_IFERR(error, "Failed to spawn and start the new thread.\n");

	vka_object_t bram_frame_object;

	/**
	 * vka_alloc_frame is used in the tutorial. We call this function which is
	 * called 2 levels internally since here we can specify the physical
	 * address we want to map and that we want to use a device
	 */
	vka_alloc_object_at_maybe_dev(	&vka, seL4_ARM_SmallPageObject,
		BRAM_SIZE_BITS, HLS_BASE_ADDR, MAP_A_DEVICE, &bram_frame_object);

	ZF_LOGF_IFERR(error, "Failed to alloc a frame in BRAM.\n");

	// vka_alloc_frame( &vka, ARRAY_SIZE_BITS, vka_object_t *result)

	// This array needs to be passed to sel4utils_map_page cause this function
	// will populate it with reference to all additional objects that were
	// created in order to map the page. num_objects will be updated with the
	// actual number of objects that will be allocated
	vka_object_t objects[2];
	int num_objects = 2;

	seL4_Word *x = (seL4_Word *) TEST_VADDR;
	error = sel4utils_map_page(	&vka, pd_cap, bram_frame_object.cptr, (void*)x,
		seL4_ReadWrite, 0, objects, &num_objects);
	ZF_LOGF_IFERR(error,
		"Failed again to map the bram frame into the VSpace.\n");


	for (i = 0; i < benchmarksNum; i++)
	{
		printf("Running %s:\n", benchName[i]);
		benchInit[i]();
		benchWarm[i](WARMUP_HEAT);
		result = benchRun[i]();
		correct = benchVerify[i](result);
		printf("Finished %s with code %d:\n", benchName[i], correct);
	}
	

	return 0;
}