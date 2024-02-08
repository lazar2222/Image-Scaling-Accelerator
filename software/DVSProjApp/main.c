#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <system.h>
#include <altera_avalon_performance_counter.h>
#include <sys/alt_cache.h>

#include "sw_impl.h"
#include "hw_impl.h"
#include "benchmark_utils.h"

#define MAX_PATH 256
#define CCC(cmd) if (checkCommand(cmd)) { continue; }

typedef struct
{
	char status;
	char fname[MAX_PATH];
	char benchmark;
	int xScale;
	int yScale;
	int x;
	int y;
	int ex;
	int ey;
	int w;
	int h;
	int sourceWidth;
	int sourceHeight;
	int destinationWidth;
	int destinationHeight;
	int sourceSize;
	int destinationSize;
	unsigned char* sourceImage;
	unsigned char* referenceImage;
	unsigned char* destinationImage;
} Command;

void printHelp()
{
	printf("Enter command in this format <filename> (B | [R <x> <y> <w> <h>] <scale factor>)\n");
	printf("B starts benchmark, no other parameters are allowed\n");
	printf("R selects the part of the picture to scale\n");
	printf("Scale factor is one or two numbers in range {-4, -3, -2, -1, 1, 2, 3, 4}\n");
	printf("If two numbers are specified they are x and y scaling factors respectively\n");
}

void printError(Command* cmd)
{
	int status = cmd->status;
	if (status == 0) { printf("No error\n"); }
	else if (status >= 1 && status <= 5) { printf("Failed to load image\n"); }
	else if (status == 6) { printf("Invalid X scale factor\n"); }
	else if (status == 7) { printf("Invalid Y scale factor\n"); }
	else if (status == 8) { printf("Invalid picture start point\n"); }
	else if (status == 9) { printf("Invalid picture end point\n"); }
	else if (status == 10 || status == 11) { printf("Failed to allocate output buffers\n"); }
	else if (status >= 12 && status <= 15) { printf("Failed to save image\n"); }
	else { printf("Unknown error\n"); }
}

void cleanup(Command* cmd)
{
	if (cmd->sourceImage != NULL) { free(cmd->sourceImage); cmd->sourceImage = NULL; }
	if (cmd->referenceImage != NULL) { free(cmd->referenceImage); cmd->referenceImage = NULL; }
	if (cmd->destinationImage != NULL) { free(cmd->destinationImage); cmd->destinationImage = NULL; }
}

int checkCommand(Command* cmd)
{
	if (cmd->status != 0)
	{
		printError(cmd);
		cleanup(cmd);
		return 1;
	}
	return 0;
}

Command parseCommand()
{
	char next;
	Command cmd;

	cmd.status = 0;
	cmd.benchmark = 0;
	cmd.xScale = 0;
	cmd.yScale = 0;
	cmd.x = -1;
	cmd.y = -1;
	cmd.ex = -1;
	cmd.ey = -1;
	cmd.w = -1;
	cmd.h = -1;
	cmd.sourceWidth = -1;
	cmd.sourceHeight = -1;
	cmd.destinationWidth = -1;
	cmd.destinationHeight = -1;
	cmd.sourceImage = NULL;
	cmd.referenceImage = NULL;
	cmd.destinationImage = NULL;

	scanf("%s", cmd.fname);

	for (next = ' '; next == ' '; next = getchar()) {}

	if (next == 'B') { cmd.benchmark = 1; return cmd; }
	else if (next == 'R') { scanf("%d %d %d %d", &cmd.x, &cmd.y, &cmd.w,  &cmd.h); }
	else { ungetc(next, stdin); }

	scanf("%d", &cmd.xScale);
	cmd.yScale = cmd.xScale;

	for (next = ' '; next == ' '; next = getchar()) {}

	if (next == '\n' || next == '\r') { return cmd; }
	else { ungetc(next, stdin); }

	scanf("%d", &cmd.yScale);

	return cmd;
}

void loadImage(Command* cmd)
{
	char ffname[MAX_PATH] = "/mnt/host/../../";
	strcat(ffname, cmd->fname);
	FILE* f = fopen(ffname, "rb");
	if (f == NULL) { cmd->status = 1; return; }

	size_t read = fread(&cmd->sourceWidth, sizeof(int), 1, f);
	if (read != 1) { fclose(f); cmd->status = 2; return; }

	read = fread(&cmd->sourceHeight, sizeof(int), 1, f);
	if (read != 1) { fclose(f); cmd->status = 3; return; }

	cmd->sourceSize = cmd->sourceWidth * cmd->sourceHeight;
	cmd->sourceImage = malloc(sizeof(unsigned char) * cmd->sourceSize);
	if (cmd->sourceImage == NULL) { fclose(f); cmd->status = 4; return; }

	read = fread(cmd->sourceImage, sizeof(unsigned char), cmd->sourceSize, f);
	if (read != cmd->sourceSize) { fclose(f); cmd->status = 5; return; }

	fclose(f);
}

void prepareCommand(Command* cmd)
{
	int validScales[] = {-4, -3, -2, -1, 1, 2, 3, 4};

	int validX = 0, validY = 0;
	for (int i = 0; i < 8; i++)
	{
		if (cmd->xScale == validScales[i]) { validX = 1; }
		if (cmd->yScale == validScales[i]) { validY = 1; }
	}
	if (validX == 0) { cmd->status = 6; return; }
	if (validY == 0) { cmd->status = 7; return; }

	if (cmd->x == -1) { cmd->x = 0; }
	if (cmd->y == -1) { cmd->y = 0; }
	if (cmd->w == -1) { cmd->w = cmd->sourceWidth; }
	if (cmd->h == -1) { cmd->h = cmd->sourceHeight; }

	cmd->ex = cmd->x + cmd->w;
	cmd->ey = cmd->y + cmd->h;

	if (cmd->x < 0 || cmd->x >= cmd->sourceWidth || cmd->y < 0 || cmd->y >= cmd->sourceHeight) { cmd->status = 8; return; }
	if (cmd->ex <= 0 || cmd->ex > cmd->sourceWidth || cmd->ey <= 0 || cmd->ey > cmd->sourceHeight) { cmd->status = 9; return; }

	cmd->destinationWidth = cmd->w;
	cmd->destinationHeight = cmd->h;

	if(cmd->xScale < 0) { cmd->destinationWidth /= -cmd->xScale; }
	else { cmd->destinationWidth *= cmd->xScale; }
	if(cmd->yScale < 0) { cmd->destinationHeight /= -cmd->yScale; }
	else { cmd->destinationHeight *= cmd->yScale; }

	cmd->destinationSize = cmd->destinationWidth * cmd->destinationHeight;

	cmd->referenceImage = malloc(sizeof(unsigned char) * cmd->destinationSize);
	cmd->destinationImage = malloc(sizeof(unsigned char) * cmd->destinationSize);

	if (cmd->referenceImage == NULL) { cmd->status = 10; return; }
	if (cmd->destinationImage == NULL) { cmd->status = 11; return; }
}

void resizeImage(Command* cmd)
{
	PERF_RESET(PERF_CNT_BASE);
	PERF_START_MEASURING(PERF_CNT_BASE);

	alt_dcache_flush_all();
	PERF_BEGIN(PERF_CNT_BASE, 1);
	scaleSW(cmd->sourceImage, cmd->referenceImage, cmd->sourceWidth, cmd->sourceHeight, cmd->x, cmd->y, cmd->w, cmd->h, cmd->xScale, cmd->yScale);
	PERF_END(PERF_CNT_BASE, 1);

	alt_dcache_flush_all();
	PERF_BEGIN(PERF_CNT_BASE, 2);
	checkHWStatus(scaleHW(cmd->sourceImage, cmd->destinationImage, cmd->sourceWidth, cmd->sourceHeight, cmd->x, cmd->y, cmd->w, cmd->h, cmd->xScale, cmd->yScale));
	PERF_END(PERF_CNT_BASE, 3);
}

void saveImage(Command* cmd)
{
	char ffname[MAX_PATH] = "/mnt/host/../../";
	strcat(ffname, cmd->fname);
	int dot = strlen(ffname);
	for (; ffname[dot] != '.'; dot--) {}
	ffname[dot + 1] = 'o';
	ffname[dot + 2] = 'u';
	ffname[dot + 3] = 't';
	ffname[dot + 4] = 0;

	FILE* f = fopen(ffname, "wb");
	if (f == NULL) { cmd->status = 12; return; }

	size_t write = fwrite(&cmd->destinationWidth, sizeof(int), 1, f);
	if (write != 1) { fclose(f); cmd->status = 13; return; }

	write = fwrite(&cmd->destinationHeight, sizeof(int), 1, f);
	if (write != 1) { fclose(f); cmd->status = 14; return; }

	write = fwrite(cmd->destinationImage, sizeof(unsigned char), cmd->destinationSize, f);
	if (write != cmd->destinationSize) { fclose(f); cmd->status = 15; return; }

	fclose(f);
}

int main()
{
	Command command;
	Command* cmd = &command;

	checkHWStatus(initHW());

	printHelp();

	while(1)
	{
		command = parseCommand();

		loadImage(cmd);
		CCC(cmd);
		printf("Image loaded\n");

		int a;
		scanf("%d", &a);

		if (cmd->benchmark)
		{

		}
		else
		{
			prepareCommand(cmd);
			CCC(cmd);

			resizeImage(cmd);
			CCC(cmd);
			printf("Image resized\n");
			scanf("%d", &a);
			saveImage(cmd);
			CCC(cmd);
			printf("Image saved\n");
		}

		cleanup(cmd);
	}

	return 0;
}
