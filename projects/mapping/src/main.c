#include <stdio.h>
#include <sel4/sel4.h>
#include <sel4platsupport/bootinfo.h>
#include <utils/util.h>
#include <sel4/sel4_arch/mapping.h>

#define BRAM_BASE_ADDR	0xa0000000 //! The base physical address of our BRAM in the FPGA
#define BRAM_LAST_OFFSET 	16 //! 2^16 = 65 KBytes. An intermediate size needed to map, to reach BRAM address
#define BRAM_SIZE_BITS 	13 //! 2^13 = 8K Bytes. The BRAM size in bitwidth
#define TEST_VADDR		0xA0000000 //! Just a virtual address we map our BRAM to. Copied from tutorial
#define LONG_MSBIT_POS	63 //! We use that to determine the position of the MSbit of physical addresses so that we can count the bitwidth of some sizes

/**
 * The alloc_slot and alloc_object functions are copied directly from the
 * 'projects/sel4-tutorials/libsel4tutorials/src/alloc.c' file in the root
 * directory of the SeL4 tutorials.
 *
 * The alloc_objectIO is a modified alloc_object specifically implemented to
 * map an 8KB frame to our BRAM.
 */
seL4_CPtr alloc_slot(seL4_BootInfo *info);
seL4_CPtr alloc_object(seL4_BootInfo *info, seL4_Word type, seL4_Word size_bits);
seL4_CPtr alloc_objectIO(seL4_BootInfo *info, seL4_Word type, seL4_Word size_bits);

int main(void)
{
	seL4_BootInfo *info = platsupport_get_bootinfo();

	seL4_CPtr unused_untyped = info->empty.start;

	/**
	 * This loop is copied from the 'untyped' tutorial. It prints all the
	 * available Untyped and Device Untyped addresses of our system
	 */
	printf("    CSlot   \tPaddr           \tSize\tType\n");
	for (seL4_CPtr slot = info->untyped.start; slot != info->untyped.end; slot++)
	{
		seL4_UntypedDesc *desc = &info->untypedList[slot - info->untyped.start];
		printf(	"%8p\t%16p\t2^%d\t%s\n",
				(void *) slot,
				(void *) desc->paddr,
				desc->sizeBits,
				desc->isDevice ? "device untyped" : "untyped");
	}
	seL4_Error error;

	/*
	 * Unlike the mapping tutorial we call alloc_objectIO to map the frame to
	 * BRAM. We also allocate Aarch64-specific objects.
	 */
	seL4_CPtr frame = alloc_objectIO(info, seL4_ARM_SmallPageObject, 0);
	seL4_CPtr pud = alloc_object(info, seL4_ARM_HugePageObject, 0); //For 64 bits
	seL4_CPtr pd = alloc_object(info, seL4_ARM_PageDirectoryObject, 0);
	seL4_CPtr pt = alloc_object(info, seL4_ARM_PageTableObject, 0);

	/**
	 * After we allocate the objects, we map them to our VSpace we a specified
	 * virtual address. Again we use Aarch64-specific definitions.
	 */
	error = seL4_ARM_PageUpperDirectory_Map(pud, seL4_CapInitThreadVSpace,
		TEST_VADDR, seL4_ARM_Default_VMAttributes);

	error = seL4_ARM_PageDirectory_Map(pd, seL4_CapInitThreadVSpace,
		TEST_VADDR, seL4_ARM_Default_VMAttributes);
	assert(error == seL4_NoError);

	error = seL4_ARM_PageTable_Map(pt, seL4_CapInitThreadVSpace,
		TEST_VADDR, seL4_ARM_Default_VMAttributes);
	assert(error == seL4_NoError);

	error = seL4_ARM_Page_Map(frame, seL4_CapInitThreadVSpace,
		TEST_VADDR, seL4_ReadWrite, seL4_ARM_Default_VMAttributes);
	assert(error == seL4_NoError);

	seL4_Word *x = (seL4_Word *) TEST_VADDR;
	/* write to the page we mapped */
	printf("Set x to 5\n");
	*x = 5;
	//printf("Read x: %lu\n", *x);


	printf("Hello at last!\n");
	return 0;
}

seL4_CPtr alloc_slot(seL4_BootInfo *info)
{
	ZF_LOGF_IF(info->empty.start == info->empty.end, "No CSlots left!");
	seL4_CPtr next_free_slot = info->empty.start++;
	return next_free_slot;
}

/* a very simple allocation function that iterates through the untypeds in boot info until
   a retype succeeds */
seL4_CPtr alloc_object(seL4_BootInfo *info, seL4_Word type, seL4_Word size_bits)
{
	seL4_CPtr cslot = alloc_slot(info);

	/* keep trying to retype until we succeed */
	seL4_Error error = seL4_NotEnoughMemory;
	for (seL4_CPtr untyped = info->untyped.start; untyped < info->untyped.end; untyped++) {
		seL4_UntypedDesc *desc = &info->untypedList[untyped - info->untyped.start];
		if (!desc->isDevice) {
			seL4_Error error = seL4_Untyped_Retype(untyped, type, 0,
				seL4_CapInitThreadCNode, 0, 0, cslot, 1);
			if (error == seL4_NoError) {
				return cslot;
			} else if (error != seL4_NotEnoughMemory) {
				ZF_LOGF_IF(error != seL4_NotEnoughMemory, "Failed to allocate untyped");
			}
		}
	}

	ZF_LOGF_IF(error == seL4_NotEnoughMemory, "Out of untyped memory");
	return cslot;
}

seL4_CPtr alloc_objectIO(seL4_BootInfo *info, seL4_Word type, seL4_Word size_bits)
{
	seL4_CPtr cslot = alloc_slot(info);

	/* keep trying to retype until we succeed */
	seL4_Error error = seL4_NotEnoughMemory;
	for (seL4_CPtr untyped = info->untyped.start; untyped < info->untyped.end; untyped++)
	{
		seL4_UntypedDesc *desc = &info->untypedList[untyped - info->untyped.start];
		if (desc->isDevice)
		{
			// If The Kernel device Untyped starts before BRAM and Ends after BRAM
			if ( (desc->paddr < BRAM_BASE_ADDR) && (BRAM_BASE_ADDR < (desc->paddr + BIT(desc->sizeBits))) )
			{
				
				// calculate size that will allocate from the beginning of this
				// device untyped region(here 0x8000_0000) to 0xA000_0000.
				seL4_Word sizeToBram = LONG_MSBIT_POS - __builtin_clzl(BRAM_BASE_ADDR - desc->paddr);

				// retype memory till 0xA0000000
				error = seL4_Untyped_Retype(untyped, seL4_UntypedObject, sizeToBram,
					seL4_CapInitThreadCNode, 0,	0, cslot, 1	);
				ZF_LOGF_IF(error != seL4_NoError, "Failed to allocate untyped before BRAM");

				// allocate another slot and retype till 0xA001_0000
				cslot = alloc_slot(info);
				error = seL4_Untyped_Retype(untyped, seL4_UntypedObject, BRAM_LAST_OFFSET,
					seL4_CapInitThreadCNode, 0, 0, cslot, 1);
				ZF_LOGF_IF(error != seL4_NoError, "Failed to allocate untyped before BRAM");

				// finally retype from 0xA001_0000 to 0xA001_2000
				cslot = alloc_slot(info);
				error = seL4_Untyped_Retype(untyped, type, BRAM_SIZE_BITS,
					seL4_CapInitThreadCNode, 0, 0, cslot, 1);
				ZF_LOGF_IF(error != seL4_NoError, "Failed to allocate  BRAM 1");
			}
			// If The Kernel device untyped starts at same offset as BRAM
			else if (desc->paddr == BRAM_BASE_ADDR)
			{
				// just retype our frame immediately.
				seL4_Error error = seL4_Untyped_Retype(	untyped, type, BRAM_SIZE_BITS,
					seL4_CapInitThreadCNode, 0, 0, cslot, 1);
				ZF_LOGF_IF(error != seL4_NoError, "Failed to allocate  BRAM 2");
			}
			if (error == seL4_NoError) return cslot;
			else if (error != seL4_NotEnoughMemory)
			{
				ZF_LOGF_IF(error != seL4_NotEnoughMemory, "Failed to allocate untyped");
			}
		}
	}
	ZF_LOGF_IF(error == seL4_NotEnoughMemory, "Out of untyped memory");
	return cslot;
}
