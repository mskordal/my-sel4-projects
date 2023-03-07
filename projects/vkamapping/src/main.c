#include <stdio.h>
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


#define BRAM_BASE_ADDR	0xa0010000
#define BRAM_SIZE_BITS 	13 // 2^16 = 8 KBytes
#define TEST_VADDR		0x7000000 // this virtual address works
#define MAP_A_DEVICE	true

#define APP_PRIORITY seL4_MaxPrio
#define APP_IMAGE_NAME "app"

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

/* stack for the new thread */
#define THREAD_2_STACK_SIZE 4096
UNUSED static int thread_2_stack[THREAD_2_STACK_SIZE];


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
	ZF_LOGF_IF(virtual_reservation.res == NULL, "Failed to reserve a chunk of memory.\n");
	
	bootstrap_configure_virtual_pool(allocman, vaddr,
		ALLOCATOR_VIRTUAL_POOL_SIZE, simple_get_pd(&simple));


	/* use sel4utils to make a new process */
	sel4utils_process_t new_process;

	sel4utils_process_config_t config = process_config_default_simple(&simple, APP_IMAGE_NAME, APP_PRIORITY);
	error = sel4utils_configure_process_custom(&new_process, &vka, &vspace, config);
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
	error = sel4utils_spawn_process_v(&new_process, &vka, &vspace, argc, (char**) &argv, 1);
	ZF_LOGF_IFERR(error, "Failed to spawn and start the new thread.\n");


	vka_object_t bram_frame_object;
	/**
	 * vka_alloc_frame is used in the tutorial. We call this function which is
	 * called 2 levels internally since here we can specify the physical
	 * address we want to map and that we want to use a device
	 */
	vka_alloc_object_at_maybe_dev(	&vka, seL4_ARM_SmallPageObject, BRAM_SIZE_BITS,
									BRAM_BASE_ADDR, MAP_A_DEVICE, &bram_frame_object);

	ZF_LOGF_IFERR(error, "Failed to alloc a frame in BRAM.\n");

	/*with vka we only need to allocate the page table entry of the last level*/
	vka_object_t pt_object;
	error =  vka_alloc_page_table(&vka, &pt_object);
	ZF_LOGF_IFERR(error, "Failed to allocate new page table.\n");

	/*void* bram;*/
	/*reservation_t bram_reservation;*/
	/*bram_reservation = vspace_reserve_range(&vspace,*/
	/*BIT(BRAM_SIZE_BITS), seL4_ReadWrite, 1, &bram);*/
	/*ZF_LOGF_IF(virtual_reservation.res == NULL, "Failed to reserve BRAM v. memory.\n");*/
	/*printf("BRAM vaddr: %p\n", bram);*/
	/*fflush(stdout);*/


	/*here we map the PTE to our vspace*/
	error = seL4_ARCH_PageTable_Map(pt_object.cptr, pd_cap, TEST_VADDR,
									seL4_ARCH_Default_VMAttributes);
	ZF_LOGF_IFERR(error, "Failed to map page table into VSpace.\n");

	/*finally we map the page*/
	error = seL4_ARCH_Page_Map(	bram_frame_object.cptr, pd_cap, TEST_VADDR,
								seL4_ReadWrite, seL4_ARCH_Default_VMAttributes);
	ZF_LOGF_IFERR(error, "Failed again to map the bram frame into the VSpace.\n");

	/* write to the page we mapped */
	seL4_Word *x = (seL4_Word *) TEST_VADDR;
	printf("Set x to 5\n");
	*x = 5;
	printf("Read x: %lu\n", *x);
	return 0;
}
