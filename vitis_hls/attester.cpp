#include <ap_int.h>

#include "stack.h"
#include "attester_types.h"

#define BRAM_BASE_ADDR 0xa0010000
#define CALL_GRAPH_OFFSET 0
#define STACK_OFFSET 0x1c00

#define FUNCTION_END 0
#define MAIN_BEGIN 1

#define LEVEL_OFFSET 0
#define ID_OFFSET 1
#define GRAPH_INDEX_NEXT 2

/**
 * Graph node attributes:
 * 	ap_int<8> level;
	ap_int<8> id;
 */


void attester_top_func(ap_uint<8> id, ap_uint<8>* bram)
{
#pragma HLS INTERFACE mode=s_axilite bundle=BUS_A port=id
#pragma HLS INTERFACE m_axi port=bram bundle=BUS_B
#pragma HLS INTERFACE s_axilite port=return bundle=BUS_A

	//	static node_t* stack[16];
	graph_index_t* stack = (graph_index_t*) (bram + STACK_OFFSET);
	//	static call_graph_node_t call_graph[256];
	ap_uint<8>* call_graph = (ap_uint<8>*) (bram + CALL_GRAPH_OFFSET);

	graph_index_t parent_index;

	static stack_index_t sp = 0;
	static graph_index_t graph_index = 0;

	/**
	 * Reached the end of a function
	 */
	if(id == FUNCTION_END)
	{
		pop(stack, sp);
		return;
	}
	/**
	 * In the beginning of main function
	 */
//	else if(id == MAIN_BEGIN)
//	{
//		call_graph[graph_index + LEVEL_OFFSET] = 0;
//	}
	/**
	 * In the beginning of any other function
	 */
	else
	{
		parent_index = peek(stack, sp);
		if(parent_index == -1)
			call_graph[graph_index + LEVEL_OFFSET] = 1;
		else
			call_graph[graph_index + LEVEL_OFFSET] = call_graph[parent_index + LEVEL_OFFSET] + 1;
	}
	/**
	 * Set node id, push to stack and update index for next node store
	 */
	call_graph[graph_index + ID_OFFSET] = id;
    push(graph_index, stack, sp);
    graph_index = graph_index + GRAPH_INDEX_NEXT;

////TEST CODE BEGIN
//	call_graph[graph_index] = id;
////TEST CODE END
//	graph_index = graph_index + GRAPH_INDEX_NEXT;

}

