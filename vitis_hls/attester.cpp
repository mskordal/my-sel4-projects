#include <ap_int.h>

#include "stack.h"
#include "attester_types.h"

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


void attester_top_func(ap_uint<64> nodeData, ap_uint<64> cpuCycles,
	ap_uint<64> event0, ap_uint<64> event1, ap_uint<64> event2,
	ap_uint<64> event3, ap_uint<64> event4, ap_uint<64> event5,
	ap_uint<8>* bram)
{
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=nodeData
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=cpuCycles
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event0
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event1
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event2
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event3
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event4
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event5
#pragma HLS INTERFACE m_axi port=bram bundle=BUS_B
#pragma HLS INTERFACE s_axilite port=return bundle=BUS_A

	//	static node_t* stack[16];
	graph_index_t* stack = (graph_index_t*) (bram + STACK_OFFSET);
	//	static call_graph_node_t call_graph[256];
	call_graph_t* call_graph = (call_graph_t*) (bram + CALL_GRAPH_OFFSET);


	graph_index_t parent_index;

	static stack_index_t sp = 0;
	static graph_index_t graph_index = 0;

	// In the function start, we store the ID of function and events and set the
	// function level based on parent.
	if((nodeData & FUNC_ID_MASK) )
	{
		//parent_index = peek(stack, sp);
		if (sp > 0)
		{
			parent_index = stack[sp-1];
		}
		else
		{
			parent_index = -1;
		}
		if(parent_index == -1)
		{
			nodeData |= 0x100000000000000;
		}
		else //get level from parent, add + 1 a
		{
			nodeData |=  ((ap_uint<64>)(((call_graph[parent_index + NODE_DATA_MS_OFFSET] & LEVEL_MASK) + 0x1000000)) << 32);
		}
		call_graph[graph_index + NODE_DATA_LS_OFFSET] = nodeData;
		call_graph[graph_index + NODE_DATA_MS_OFFSET] = nodeData >> 32;

		//push(graph_index, stack, sp);
		stack[sp] = graph_index;
		++sp;

		graph_index = graph_index + GRAPH_INDEX_NEXT;
	}
	else
	{
		// In the function end, we store the event values.
		//parent_index = pop(stack, sp);
		if (sp > 0)
		{
			--sp;
			parent_index = stack[sp];
		}
		else
		{
			parent_index = -1;
		}
		call_graph[parent_index + NODE_CYCLES_LS_OFFSET] = cpuCycles;
		call_graph[parent_index + NODE_CYCLES_MS_OFFSET] = cpuCycles >> 32;

		call_graph[parent_index + NODE_EVENT0_LS_OFFSET] = event0;
		call_graph[parent_index + NODE_EVENT0_MS_OFFSET] = event0 >> 32;

		call_graph[parent_index + NODE_EVENT1_LS_OFFSET] = event1;
		call_graph[parent_index + NODE_EVENT1_MS_OFFSET] = event1 >> 32;

		call_graph[parent_index + NODE_EVENT2_LS_OFFSET] = event2;
		call_graph[parent_index + NODE_EVENT2_MS_OFFSET] = event2 >> 32;

		call_graph[parent_index + NODE_EVENT3_LS_OFFSET] = event3;
		call_graph[parent_index + NODE_EVENT3_MS_OFFSET] = event3 >> 32;

		call_graph[parent_index + NODE_EVENT4_LS_OFFSET] = event4;
		call_graph[parent_index + NODE_EVENT4_MS_OFFSET] = event4 >> 32;

		call_graph[parent_index + NODE_EVENT5_LS_OFFSET] = event5;
		call_graph[parent_index + NODE_EVENT5_MS_OFFSET] = event5 >> 32;
	}
}