#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sel4/simple_types.h>
#include <utils/attribute.h>

#include "attester_sw.h"

#define CALL_GRAPH_OFFSET 0
#define STACK_OFFSET 0x8000

//NodeData LS word Values
#define FUNC_ID_MASK 	0x000000ff
#define EVENT0_ID_MASK 	0x0000ff00
#define EVENT1_ID_MASK 	0x00ff0000
#define EVENT2_ID_MASK 	0xff000000
//NodeData MS word Values
#define EVENT3_ID_MASK 	0x000000ff
#define EVENT4_ID_MASK 	0x0000ff00
#define EVENT5_ID_MASK 	0x00ff0000
#define LEVEL_MASK 		0xff000000

#define NODE_DATA_LS_OFFSET 0
#define NODE_DATA_MS_OFFSET 1
#define NODE_CYCLES_LS_OFFSET 2
#define NODE_CYCLES_MS_OFFSET 3
#define NODE_EVENT0_LS_OFFSET 4
#define NODE_EVENT0_MS_OFFSET 5
#define NODE_EVENT1_LS_OFFSET 6
#define NODE_EVENT1_MS_OFFSET 7
#define NODE_EVENT2_LS_OFFSET 8
#define NODE_EVENT2_MS_OFFSET 9
#define NODE_EVENT3_LS_OFFSET 10
#define NODE_EVENT3_MS_OFFSET 11
#define NODE_EVENT4_LS_OFFSET 12
#define NODE_EVENT4_MS_OFFSET 13
#define NODE_EVENT5_LS_OFFSET 14
#define NODE_EVENT5_MS_OFFSET 15
#define GRAPH_INDEX_NEXT 16

seL4_Uint16 sp = 0;
seL4_Uint16 graph_index = 0;
seL4_Uint16 stack[1024];
seL4_Uint32 call_graph[32768];


void attester_top_func( seL4_Uint32 nodeDataLs, seL4_Uint32 nodeDataMs,
	seL4_Uint32 cpuCyclesLs, seL4_Uint32 cpuCyclesMs, seL4_Uint32 event0Ls,
	seL4_Uint32 event0Ms, seL4_Uint32 event1Ls, seL4_Uint32 event1Ms,
	seL4_Uint32 event2Ls, seL4_Uint32 event2Ms, seL4_Uint32 event3Ls,
	seL4_Uint32 event3Ms, seL4_Uint32 event4Ls, seL4_Uint32 event4Ms,
	seL4_Uint32 event5Ls, seL4_Uint32 event5Ms )
{
	seL4_Uint16 parent_index;

	// In the function start, we store the ID of function and events and set the
	// function level based on parent.
	if(nodeDataLs != 0)
	{
		if (sp > 0)
		{
			parent_index = stack[sp-1];
			nodeDataMs = call_graph[parent_index + NODE_DATA_MS_OFFSET] + 0x1;
		}
		else
		{
			nodeDataMs = nodeDataMs | 0x1;
		}
		call_graph[graph_index + NODE_DATA_LS_OFFSET] = nodeDataLs;
		call_graph[graph_index + NODE_DATA_MS_OFFSET] = nodeDataMs;

		stack[sp] = graph_index;
		sp = sp + 1;

		graph_index = graph_index + GRAPH_INDEX_NEXT;
	}
	else //if (sp > 0)
	{
		sp = sp - 1;
		parent_index = stack[sp];

		call_graph[parent_index + NODE_CYCLES_LS_OFFSET] = cpuCyclesLs;
		call_graph[parent_index + NODE_CYCLES_MS_OFFSET] = cpuCyclesMs;

		call_graph[parent_index + NODE_EVENT0_LS_OFFSET] = event0Ls;
		call_graph[parent_index + NODE_EVENT0_MS_OFFSET] = event0Ms;

		call_graph[parent_index + NODE_EVENT1_LS_OFFSET] = event1Ls;
		call_graph[parent_index + NODE_EVENT1_MS_OFFSET] = event1Ms;

		call_graph[parent_index + NODE_EVENT2_LS_OFFSET] = event2Ls;
		call_graph[parent_index + NODE_EVENT2_MS_OFFSET] = event2Ms;

		call_graph[parent_index + NODE_EVENT3_LS_OFFSET] = event3Ls;
		call_graph[parent_index + NODE_EVENT3_MS_OFFSET] = event3Ms;

		call_graph[parent_index + NODE_EVENT4_LS_OFFSET] = event4Ls;
		call_graph[parent_index + NODE_EVENT4_MS_OFFSET] = event4Ms;

		call_graph[parent_index + NODE_EVENT5_LS_OFFSET] = event5Ls;
		call_graph[parent_index + NODE_EVENT5_MS_OFFSET] = event5Ms;
	}
}

void attester_print(void)
{
	seL4_Uint16 i, j, fid, flvl;
	seL4_Uint64 cpuCycles, event0, event1, event2, event3, event4, event5;

	printf
	(
		"Printing call graph raw format\n"
		"------------------------------\n\n"
	);
	for ( i = 0; i < graph_index; ++i)
	{
		printf("%p: 0x%x\n", &call_graph[i], call_graph[i]);
	}
	printf("\n");

	printf
	(
		"Printing call graph in csv format\n"
		"---------------------------------\n\n"
	);
	printf(	"function-id,function-lvl,cpu-cycles,event-0,event-1,event-2,"
			"event-3,event-4,event-5\n");
	
	for ( i = 0; i < graph_index; i += GRAPH_INDEX_NEXT)
	{
		fid = call_graph[i+NODE_DATA_LS_OFFSET] & 0xff;
		flvl = call_graph[i+NODE_DATA_MS_OFFSET] & 0xff;

		cpuCycles = (seL4_Uint64) call_graph[i+NODE_CYCLES_MS_OFFSET] << 32 |
			(seL4_Uint64) call_graph[i+NODE_CYCLES_LS_OFFSET];
		event0 = (seL4_Uint64) call_graph[i+NODE_EVENT0_MS_OFFSET] << 32 |
			(seL4_Uint64) call_graph[i+NODE_EVENT0_LS_OFFSET];
		event1 = (seL4_Uint64) call_graph[i+NODE_EVENT1_MS_OFFSET] << 32 |
			(seL4_Uint64) call_graph[i+NODE_EVENT1_LS_OFFSET];
		event2 = (seL4_Uint64) call_graph[i+NODE_EVENT2_MS_OFFSET] << 32 |
			(seL4_Uint64) call_graph[i+NODE_EVENT2_LS_OFFSET];
		event3 = (seL4_Uint64) call_graph[i+NODE_EVENT3_MS_OFFSET] << 32 |
			(seL4_Uint64) call_graph[i+NODE_EVENT3_LS_OFFSET];
		event4 = (seL4_Uint64) call_graph[i+NODE_EVENT4_MS_OFFSET] << 32 |
			(seL4_Uint64) call_graph[i+NODE_EVENT4_LS_OFFSET];
		event5 = (seL4_Uint64) call_graph[i+NODE_EVENT5_MS_OFFSET] << 32 |
			(seL4_Uint64) call_graph[i+NODE_EVENT5_LS_OFFSET];
		
		printf("%hu,%hu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n", fid, flvl, cpuCycles,
			event0, event1, event2, event3, event4, event5);
	}
}