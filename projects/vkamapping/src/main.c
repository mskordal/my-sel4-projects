#include <stdio.h>
#include <stdlib.h>
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

double drand(double min, double max)
{ //
    double random_double = (double) rand() / RAND_MAX; 
    random_double = (random_double * (max - min)) + min;
    return random_double;
}

void print_matrix(double mat[][DIMENSION_SIZE], int size)
{
	int i, j;
	for(i=0; i<size; i++)
	{
		for(j=0; j<size; j++)
		{
			printf("%f ", mat[i][j]);
		}
		printf("\n");
	}
}

void do_block(int si, int sj, int sk, double A[][DIMENSION_SIZE],
	double B[][DIMENSION_SIZE], double C[][DIMENSION_SIZE])
{
	int i, j, k;
	// printf("do_block: si %u sj %u sk %u\n", si, sj, sk);
	for (i=si; i<si+BLOCK_SIZE; i++)
	{
		for (j=sj; j<sj+BLOCK_SIZE; j++)
		{
			double C_ij = C[i][j];
			for (k=sk; k<sk+BLOCK_SIZE; k++)
			{
				// printf("i %u, j %u, k %u\n", i, j, k);
				C_ij += A[i][k] * B[k][j]; 
            }
			C[i][j] = C_ij;
        }
    }	 
}

void blocked_matrix_multiply(double A[][DIMENSION_SIZE],
	double B[][DIMENSION_SIZE], double C[][DIMENSION_SIZE], int size)
{
	int si, sj, sk;
	/* iterate over the rows of A */
	for(sj=0; sj<size; sj+=BLOCK_SIZE)
	{
		/* Iterate over the columns of B */
		for(si=0; si<size; si+=BLOCK_SIZE)
		{
			/* Iterate over the rows of B */
			for(sk=0; sk<size; sk+=BLOCK_SIZE)
			{
				do_block(si, sj, sk, A, B, C);
			}
		}
	}
}


void matrix_multiply(double A[][DIMENSION_SIZE], double B[][DIMENSION_SIZE],
	double C[][DIMENSION_SIZE], int size)
{
	int i,j,k;
    /* iterate over the rows of A */
    for(i=0; i<size; i++) {
        /* Iterate over the columns of B */
        for(j=0; j<size; j++) {
            /* Iterate over the rows of B */
            for(k=0; k<size; k++){
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

static void matrix()
{
	// ######################################################
    // Read and check program arguments  
    // ######################################################
    // struct timeval start, stop, total;

	double A[DIMENSION_SIZE][DIMENSION_SIZE];
	double B[DIMENSION_SIZE][DIMENSION_SIZE];
	double C[DIMENSION_SIZE][DIMENSION_SIZE];// = { 0 };

    int i, j;
	unsigned myseed = 0;
    // srand(myseed);

	for(i=0; i<DIMENSION_SIZE; i++)
    {
	    for(j=0; j<DIMENSION_SIZE; j++)
		{
            A[i][j] = drand(MINd, MAXd);
            B[i][j] = drand(MINd, MAXd);
            C[i][j] = drand(MINd, MAXd);
		}
	}
	// printf("\nOutput A:\n");
	// print_matrix(A, DIMENSION_SIZE);
	// printf("\nOutput B:\n");
	// print_matrix(B, DIMENSION_SIZE);

	// blocked_matrix_multiply(A, B, C, DIMENSION_SIZE);
	matrix_multiply(A, B, C, DIMENSION_SIZE);
	// printf("\nOutput C:\n");
	// print_matrix(C, DIMENSION_SIZE);
}

// static void D() { matrix(); }
// static void Y() { matrix(); D(); matrix();}
// static void X() { matrix(); Y(); matrix();}
// static void C() { matrix(); D();matrix(); X();matrix();}
// static void B() { matrix(); matrix();C(); matrix();}
// static void S() { matrix(); D(); matrix();matrix();}
// static void P() { matrix(); S(); matrix();}
// static void O() { matrix(); P(); matrix();}
// static void N() { matrix(); O(); matrix();}
// static void M() { matrix(); N(); matrix();}
// static void G() { matrix(); M(); matrix();}
// static void A() { matrix(); B(); matrix();G(); matrix();}

static void D() {VARS CALC(1)}
static void Y() {VARS CALC(1) D(); CALC(3)}
static void X() {VARS CALC(1) Y(); CALC(1)}

static void C() {VARS CALC(3) D(); CALC(3) X(); CALC(9)}
static void B() {VARS CALC(5) C(); CALC(7) }
static void S() {VARS CALC(2) D(); CALC(9) }
static void P() {VARS CALC(8) S(); CALC(3) }
static void O() {VARS CALC(3) P(); CALC(1) }
static void N() {VARS CALC(2) O(); CALC(6) }
static void M() {VARS CALC(7) N(); CALC(8) }
static void G() {VARS CALC(3) M(); CALC(4) }
static void A() {VARS CALC(7) B(); CALC(2) G(); CALC(6)}
int main(void)
{
	seL4_Error error = 0;

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

	printf("Application is using pass!!\n");

	// attester_top_func(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);

	A();
	//Counter initialization
	// sel4bench_init();
	// sel4bench_set_count_event(0, SEL4BENCH_EVENT_CACHE_L1D_MISS);
	// sel4bench_set_count_event(1, SEL4BENCH_EVENT_CACHE_L1D_HIT);
	// sel4bench_set_count_event(2, SEL4BENCH_EVENT_TLB_L1D_MISS);
	// sel4bench_set_count_event(3, SEL4BENCH_EVENT_MEMORY_READ);
	// sel4bench_set_count_event(4, SEL4BENCH_EVENT_MEMORY_WRITE);
	// sel4bench_set_count_event(5, SEL4BENCH_EVENT_BRANCH_MISPREDICT);
	// sel4bench_reset_counters();
	// sel4bench_start_counters(0x3F);
	// SEL4BENCH_RESET_CCNT;
	


	/*ccnt_t cpu_cycles = sel4bench_get_cycle_count();*/
	/*ccnt_t l1d_cache_ref = sel4bench_get_counter(0);*/
	/*ccnt_t l1d_cache_acc = sel4bench_get_counter(1);*/
	/*ccnt_t l1d_tlb_ref = sel4bench_get_counter(2);*/
	/*ccnt_t ld_retired = sel4bench_get_counter(3);*/
	/*ccnt_t st_retired = sel4bench_get_counter(4);*/
	/*ccnt_t br_mis_pred = sel4bench_get_counter(5);*/

	/*printf(*/
		/*"CPU cycles: %lu\n"*/
		/*"Cache L1D refills: %lu\n"*/
		/*"Cache L1D accesses: %lu\n"*/
		/*"TLB L1D refills: %lu\n"*/
		/*"Loads executed: %lu\n"*/
		/*"Stores executed: %lu\n"*/
		/*"Mispredicted branches: %lu\n",*/
		/*cpu_cycles, l1d_cache_ref, l1d_cache_acc, l1d_tlb_ref, ld_retired,*/
		/*st_retired, br_mis_pred);*/

	// seL4_Word num_counters = sel4bench_get_num_counters();
	// ccnt_t cycle_count = sel4bench_get_cycle_count();

	// printf(
	// 	"%s\n"
	// 	"%s\n"
	// 	"%s\n"
	// 	"%s\n"
	// 	"%s\n"
	// 	"%s\n",
	// 	sel4bench_get_counter_description(0),
	// 	sel4bench_get_counter_description(1),
	// 	sel4bench_get_counter_description(2),
	// 	sel4bench_get_counter_description(3),
	// 	sel4bench_get_counter_description(4),
	// 	sel4bench_get_counter_description(5));
	
	// printf(
	// 	"number of available counters: %lu\n"
	// 	"cycles counted in the beginning: %lu\n",
	// 	num_counters, cycle_count);
	// printf("%s\n", sel4bench_get_counter_description(1));
	// printf("%s\n", sel4bench_get_counter_description(1));


	// // for (int i; i<1000000; i++);
	// for (int i; i<500000; i++);
	// cycle_count = sel4bench_get_cycle_count();
	// printf("cycles counted after loop: %lu\n", cycle_count);
	/*sel4bench_destroy();*/

	return 0;
}
