#ifndef BENCHMARK_UTILS_H_
#define BENCHMARK_UTILS_H_

#include "hw_impl.h"

int verify(unsigned char* reference, unsigned char* target, int size);
int writeImage(char* fname, unsigned char* destinationImage, int destinationWidth, int destinationHeight);
void benchmark(HWContext* ctx, char* fname, unsigned char* source, int width, int height);

#endif /* BENCHMARK_UTILS_H_ */
