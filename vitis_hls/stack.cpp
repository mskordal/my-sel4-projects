#include "stack.h"

// Function to push data onto the stack
void push(graph_index_t graph_index, graph_index_t* stack, stack_index_t &sp)
{
	if (sp < STACK_SIZE)
    	stack[sp++] = graph_index;
}

// Function to pop data from the stack
graph_index_t pop(graph_index_t* stack, stack_index_t &sp)
{
	if (sp > 0)
	{
		return stack[--sp];
	}
	return -1;
}

graph_index_t peek(graph_index_t* stack, stack_index_t &sp)
{
	if (sp > 0)
	{
		return stack[sp-1];
	}
	return -1;
}
