#ifndef STACK_H_
#define STACK_H_

#include <ap_int.h>

#include "attester_types.h"

// Define the stack size
#define STACK_SIZE 16


void push(graph_index_t graph_index, graph_index_t* stack, stack_index_t &sp);
graph_index_t pop(graph_index_t* stack, stack_index_t &sp);
graph_index_t peek(graph_index_t* stack, stack_index_t &sp);

#endif /* STACK_H_ */
