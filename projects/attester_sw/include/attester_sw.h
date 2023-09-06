#ifndef ATTESTER_SW_H_
#define ATTESTER_SW_H_

#include <sel4/simple_types.h>

void attester_top_func( seL4_Uint32 nodeDataLs, seL4_Uint32 nodeDataMs,
	seL4_Uint32 cpuCyclesLs, seL4_Uint32 cpuCyclesMs, seL4_Uint32 event0Ls,
	seL4_Uint32 event0Ms, seL4_Uint32 event1Ls, seL4_Uint32 event1Ms,
	seL4_Uint32 event2Ls, seL4_Uint32 event2Ms, seL4_Uint32 event3Ls,
	seL4_Uint32 event3Ms, seL4_Uint32 event4Ls, seL4_Uint32 event4Ms,
	seL4_Uint32 event5Ls, seL4_Uint32 event5Ms );

void attester_print(void);

#endif /* ATTESTER_SW_H_ */