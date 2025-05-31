#include <stdio.h>
#include <stdlib.h>
#include <sel4utils/process.h>


/* constants */
#define MSG_DATA 0x6161 //  arbitrary data to send

static inline void clflush(void *addr);

int main(int argc, char **argv) {

	seL4_CPtr ep = atol(argv[0]);

	// notify main thread that secondary is ready, then wait
	printf("secondary process sends and waits\n");
	seL4_Send(ep, seL4_MessageInfo_new(0, 0, 0, 0));
	seL4_Wait(ep, NULL);

	seL4_Send(ep, seL4_MessageInfo_new(0, 0, 0, 0));
	return 0;
}


/******************************/
/* Flush and Reload functions */
/******************************/
static inline void clflush(void *addr) {
	asm volatile ("DC CIVAC, %[ad]" : : [ad] "r" (addr));
	asm volatile("DSB SY");
}
