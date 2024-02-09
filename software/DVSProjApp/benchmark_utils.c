#include "benchmark_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system.h>
#include <altera_avalon_performance_counter.h>
#include <sys/alt_cache.h>

#include "sw_impl.h"
#include "hw_impl.h"

#define MAX_PATH 256

#define BENCH_CASES 8
#define TEST_CASES 50
#define TEST_REPEATS 3
#define WRITE_RESULT 1

#define RAND_INC_EXC(l, h) ((l) + ( rand() % ((h) - (l))))

typedef struct
{
	int x;
	int y;
	int w;
	int h;
	int xScale;
	int yScale;
	int ok[3 * TEST_REPEATS];
	alt_u64 times[3 * TEST_REPEATS];
} TestCase;

int verify(unsigned char* reference, unsigned char* target, int size)
{
	int res = 0;
	for (int i = 0; i < size; i++)
	{
		res += reference[i] != target[i];
	}

	return res;
}

void generateTests(TestCase* tests, int width, int height, unsigned int seed)
{
	int validScales[] = {-4, -3, -2, -1, 1, 2, 3, 4};

	srand(seed);

	for (int i = 0; i < (BENCH_CASES + TEST_CASES); i++)
	{
		TestCase* test = &tests[i];
		if (i < BENCH_CASES)
		{
			test->x = 0;
			test->y = 0;
			test->w = width;
			test->h = height;
			test->xScale = validScales[i];
			test->yScale = validScales[i];
		}
		else
		{
			test->x = RAND_INC_EXC(0, width);
			test->y = RAND_INC_EXC(0, height);
			test->w = RAND_INC_EXC(1, width  - test->x + 1);
			test->h = RAND_INC_EXC(1, height - test->y + 1);
			test->xScale = validScales[RAND_INC_EXC(0, BENCH_CASES)];
			test->yScale = validScales[RAND_INC_EXC(0, BENCH_CASES)];
		}
	}
}

void submitResult(TestCase* test, int repeat, int idx, unsigned char* reference, unsigned char* target, int size)
{
	if (repeat == 0 && idx == 0)
	{
		test->ok[repeat * 3 + idx] = 0;
	}
	else
	{
		test->ok[repeat * 3 + idx] = verify(reference, target, size);
	}
	test->times[repeat * 3 + idx] = perf_get_section_time(PERF_CNT_BASE, idx);
	PERF_START_MEASURING(PERF_CNT_BASE);
}

int writeImage(char* fname, unsigned char* destinationImage, int destinationWidth, int destinationHeight)
{
	// Prepared path to access hostfs and move to root dir
	char ffname[MAX_PATH] = "/mnt/host/../../";
	// Append entered filename
	strcat(ffname, fname);

	// Find the position of the last dot (separating the filename from the extension)
	int dot = strlen(ffname);
	for (; ffname[dot] != '.'; dot--) {}

	// Change extension to out and terminate string
	ffname[dot + 1] = 'o';
	ffname[dot + 2] = 'u';
	ffname[dot + 3] = 't';
	ffname[dot + 4] = 0;

	// Open file
	FILE* f = fopen(ffname, "wb");
	if (f == NULL) { return 12; }

	// Write destinationWidth and destinationHeight
	size_t write = fwrite(&destinationWidth, sizeof(int), 1, f);
	if (write != 1) { fclose(f); return 13; }

	write = fwrite(&destinationHeight, sizeof(int), 1, f);
	if (write != 1) { fclose(f); return 14; }

	// Write image data to file
	int destinationSize = destinationWidth * destinationHeight;
	write = fwrite(destinationImage, sizeof(unsigned char), destinationSize, f);
	if (write != destinationSize) { fclose(f); return 15; }

	fclose(f);
	return 0;
}

void writeResult(TestCase* test, char* fname, unsigned char* destinationImage, int destinationWidth, int destinationHeight)
{
	char fileNameNoExt[MAX_PATH];
	char fileName[MAX_PATH];

	// Strip extension form input file name
	strcpy(fileNameNoExt, fname);
	int dot = strlen(fileNameNoExt);
	for (; fileNameNoExt[dot] != '.'; dot--) {}
	fileNameNoExt[dot] = 0;

	sprintf(fileName, "%s_%d_%d_%d_%d_%d_%d.out", fileNameNoExt, test->x, test->y, test->w, test->h, test->xScale, test->yScale);

	printf("Writing result to %s\n", fileName);
	if (writeImage(fileName, destinationImage, destinationWidth, destinationHeight)){ printf("Failed to write result\n"); }
}

void runTests(TestCase* tests, HWContext* ctx, char* fname, unsigned char* source, int width, int height)
{
	// Calculate maximum image size and add one byte for each test case, then allocate two buffers of that size
	int destinationSize = (4 * width * 4 * height) + (BENCH_CASES + TEST_CASES);

	// Allocate two buffers for destination image, one for software and one for hardware scaling
	unsigned char* referenceImage   = malloc(sizeof(unsigned char) * destinationSize);
	unsigned char* destinationImage = malloc(sizeof(unsigned char) * destinationSize);

	if (referenceImage   == NULL) { printf("Failed to allocate output buffer\n"); return; }
	if (destinationImage == NULL) { printf("Failed to allocate output buffer\n"); return; }

	for (int i = 0; i < (BENCH_CASES + TEST_CASES); i++)
	{
		printf("Running test case %d of %d", i + 1, BENCH_CASES + TEST_CASES);

		TestCase* test = &tests[i];

		// Calculate destination image dimensions
		int destinationWidth  = test->xScale > 0 ? test->w * test->xScale : (test->w - test->xScale - 1) / -test->xScale;
		int destinationHeight = test->yScale > 0 ? test->h * test->yScale : (test->h - test->yScale - 1) / -test->yScale;

		for (int j = 0; j < TEST_REPEATS; j++)
		{
			// Reset and restart performance counter
			PERF_RESET(PERF_CNT_BASE);
			PERF_START_MEASURING(PERF_CNT_BASE);

			// Flush cache and start measuring time
			alt_dcache_flush_all();
			PERF_BEGIN(PERF_CNT_BASE, 1);

			// Run software scaler
			scaleSW(source, referenceImage, width, height, test->x, test->y, test->w, test->h, destinationWidth, destinationHeight, test->xScale, test->yScale);

			PERF_END(PERF_CNT_BASE, 1);

			// Verify and submit the result
			submitResult(test, j, 0, referenceImage, destinationImage, destinationWidth * destinationHeight);

			// Flush cache and start measuring time
			alt_dcache_flush_all();
			PERF_BEGIN(PERF_CNT_BASE, 2);

			// Run hardware scaler
			scaleHW(ctx, source, destinationImage, width, height, test->x, test->y, test->w, test->h, destinationWidth, destinationHeight, test->xScale, test->yScale);

			PERF_END(PERF_CNT_BASE, 2);

			// Verify and submit the result
			if (checkHW(ctx)) {  printf("Hardware error\n"); continue; }
			submitResult(test, j, 1, referenceImage, destinationImage, destinationWidth * destinationHeight);

			// Flush cache and start measuring time
			alt_dcache_flush_all();
			PERF_BEGIN(PERF_CNT_BASE, 3);

			// Run hardware/software scaler
			scaleHSCD(ctx, source, destinationImage, width, height, test->x, test->y, test->w, test->h, destinationWidth, destinationHeight, test->xScale, test->yScale);

			PERF_END(PERF_CNT_BASE, 3);

			// Verify and submit the result0
			if (checkHW(ctx)) {  printf("Hardware error\n"); continue; }
			submitResult(test, j, 2, referenceImage, destinationImage, destinationWidth * destinationHeight);

		}

		if (WRITE_RESULT)
		{
			writeResult(test, fname, destinationImage, destinationWidth, destinationHeight);
		}
	}

	free(referenceImage);
	free(destinationImage);
}

void writeResults(TestCase* tests, unsigned int seed)
{
	char fname[MAX_PATH];

	printf("Writing results to benchmark_%u.csv", seed);
	sprintf(fname, "/mnt/host/../../benchmark_%u.csv", seed);

	FILE* f = fopen(fname, "w");
	if (f == NULL) { printf("Failed to open output file\n"); return; }

	for (int i = 0; i < (BENCH_CASES + TEST_CASES); i++)
	{
		TestCase* test = &tests[i];

		fprintf(f, "%d,%d,%d,%d,%d,%d", test->x, test->y, test->w, test->h, test->xScale, test->yScale);

		for (int j = 0; j < 3 * TEST_REPEATS; j++)
		{
			fprintf(f, ",%f", (float)test->times[j] / (float)ALT_CPU_FREQ);
		}

		for (int j = 0; j < 3 * TEST_REPEATS; j++)
		{
			fprintf(f, ",%d", test->ok[j]);
		}

		fprintf(f,"\n");
	}

	fclose(f);
}

void benchmark(HWContext* ctx, char* fname, unsigned char* source, int width, int height)
{
	TestCase testCases[BENCH_CASES + TEST_CASES];

	unsigned int seed = perf_get_total_time(PERF_CNT_BASE);

	printf("Starting benchmark, seed: %u", seed);

	generateTests(testCases, width, height, seed);

	runTests(testCases, ctx, fname, source, width, height);

	writeResults(testCases, seed);
}
