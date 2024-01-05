#include <ap_int.h>

#include "attester_types.h"

#define CALL_GRAPH_OFFSET 0
#define STACK_OFFSET 0x3F800

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


void attester_top_func( ap_uint<32> nodeDataLs, ap_uint<32> nodeDataMs,
	ap_uint<32> cpuCyclesLs, ap_uint<32> cpuCyclesMs,
	ap_uint<32> event0Ls, ap_uint<32> event0Ms, ap_uint<32> event1Ls, ap_uint<32> event1Ms, ap_uint<32> event2Ls, ap_uint<32> event2Ms,
	ap_uint<32> event3Ls, ap_uint<32> event3Ms, ap_uint<32> event4Ls, ap_uint<32> event4Ms, ap_uint<32> event5Ls, ap_uint<32> event5Ms,
	ap_uint<8>* bram)
{
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=nodeDataLs
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=cpuCyclesLs
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=nodeDataMs
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=cpuCyclesMs
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event0Ls
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event1Ls
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event2Ls
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event3Ls
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event4Ls
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event5Ls
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event0Ms
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event1Ms
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event2Ms
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event3Ms
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event4Ms
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=event5Ms
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
	else
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