#include "benchmark_utils.h";

int verify(unsigned char* reference, unsigned char* target, int size)
{
	int res = 0;
	for(int i = 0; i < size; i++)
	{
		res += reference[i] != target[i];
	}

	return res;
}
