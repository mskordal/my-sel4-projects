#include <ap_int.h>

void increment_bram(unsigned  function_number, unsigned* bram)
{
#pragma HLS INTERFACE s_axilite port=function_number bundle=BUS_A
#pragma HLS INTERFACE m_axi port=bram bundle=BUS_B
#pragma HLS INTERFACE s_axilite port=return bundle=BUS_A

	bram[function_number] += 1;

}