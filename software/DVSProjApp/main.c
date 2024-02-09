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
	int status;
	char fname[MAX_PATH];
	int benchmark;
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
	if (status == 0)                       { printf("No error\n"); }
	else if (status >= 1 && status <= 5)   { printf("Failed to load image\n"); }
	else if (status == 6)                  { printf("Invalid X scale factor\n"); }
	else if (status == 7)                  { printf("Invalid Y scale factor\n"); }
	else if (status == 8)                  { printf("Invalid picture start point\n"); }
	else if (status == 9)                  { printf("Invalid picture end point\n"); }
	else if (status == 10 || status == 11) { printf("Failed to allocate output buffers\n"); }
	else if (status >= 12 && status <= 15) { printf("Failed to save image\n"); }
	else if (status == 16)                 { printf("Hardware error\n"); }
	else                                   { printf("Unknown error\n"); }
}

void cleanup(Command* cmd)
{
	if (cmd->sourceImage      != NULL) { free(cmd->sourceImage);      cmd->sourceImage      = NULL; }
	if (cmd->referenceImage   != NULL) { free(cmd->referenceImage);   cmd->referenceImage   = NULL; }
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

	cmd.status            = 0;
	cmd.benchmark         = 0;
	cmd.xScale            = 0;
	cmd.yScale            = 0;
	cmd.x                 = -1;
	cmd.y                 = -1;
	cmd.ex                = -1;
	cmd.ey                = -1;
	cmd.w                 = -1;
	cmd.h                 = -1;
	cmd.sourceWidth       = -1;
	cmd.sourceHeight      = -1;
	cmd.destinationWidth  = -1;
	cmd.destinationHeight = -1;
	cmd.sourceImage       = NULL;
	cmd.referenceImage    = NULL;
	cmd.destinationImage  = NULL;

	// Read filename
	scanf("%s", cmd.fname);

	// Eat up all spaces
	for (next = ' '; next == ' '; next = getchar()) {}

	// If next character is B we are in benchmark mode, return
	if (next == 'B') { cmd.benchmark = 1; return cmd; }
	// If next character is R read which part of image to resize
	else if (next == 'R') { scanf("%d %d %d %d", &cmd.x, &cmd.y, &cmd.w, &cmd.h); }
	// Else return character to buffer and proceed with reading scale factors
	else { ungetc(next, stdin); }

	// Read X scale factor and also write it to Y scale as it is the same if only one is supplied
	scanf("%d", &cmd.xScale);
	cmd.yScale = cmd.xScale;

	// Eat up all spaces
	for (next = ' '; next == ' '; next = getchar()) {}

	// If next character is end of line command is complete, return
	if (next == '\n' || next == '\r') { return cmd; }
	// Else return character to buffer and proceed with reading Y scale factor
	else { ungetc(next, stdin); }

	// Read Y scale factor
	scanf("%d", &cmd.yScale);

	return cmd;
}

void loadImage(Command* cmd)
{
	// Prepared path to access hostfs and move to root dir
	char ffname[MAX_PATH] = "/mnt/host/../../";
	// Append entered filename
	strcat(ffname, cmd->fname);

	// Open file
	FILE* f = fopen(ffname, "rb");
	if (f == NULL) { cmd->status = 1; return; }

	// Read sourceWidth and sourceHeight
	size_t read = fread(&cmd->sourceWidth, sizeof(int), 1, f);
	if (read != 1) { fclose(f); cmd->status = 2; return; }

	read = fread(&cmd->sourceHeight, sizeof(int), 1, f);
	if (read != 1) { fclose(f); cmd->status = 3; return; }

	// Calculate image size and allocate memory
	cmd->sourceSize = cmd->sourceWidth * cmd->sourceHeight;
	cmd->sourceImage = malloc(sizeof(unsigned char) * cmd->sourceSize);
	if (cmd->sourceImage == NULL) { fclose(f); cmd->status = 4; return; }

	// Read image data into allocated buffer
	read = fread(cmd->sourceImage, sizeof(unsigned char), cmd->sourceSize, f);
	if (read != cmd->sourceSize) { fclose(f); cmd->status = 5; return; }

	fclose(f);
}

void prepareCommand(Command* cmd)
{
	int validScales[] = {-4, -3, -2, -1, 1, 2, 3, 4};

	// Check if valid scale factors have been entered, report error if not
	int validX = 0;
	int validY = 0;
	for (int i = 0; i < 8; i++)
	{
		if (cmd->xScale == validScales[i]) { validX = 1; }
		if (cmd->yScale == validScales[i]) { validY = 1; }
	}
	if (validX == 0) { cmd->status = 6; return; }
	if (validY == 0) { cmd->status = 7; return; }

	// If R option was omitted x, y, w and h have default values (-1), if that is the case setup the range to encompass the whole image
	if (cmd->x == -1) { cmd->x = 0; }
	if (cmd->y == -1) { cmd->y = 0; }
	if (cmd->w == -1) { cmd->w = cmd->sourceWidth; }
	if (cmd->h == -1) { cmd->h = cmd->sourceHeight; }

	// Calculate end points of range
	cmd->ex = cmd->x + cmd->w;
	cmd->ey = cmd->y + cmd->h;

	// Check that range is inside the bounds of image
	if (cmd->x < 0   || cmd->x >= cmd->sourceWidth || cmd->y < 0   || cmd->y >= cmd->sourceHeight) { cmd->status = 8; return; }
	if (cmd->ex <= 0 || cmd->ex > cmd->sourceWidth || cmd->ey <= 0 || cmd->ey > cmd->sourceHeight) { cmd->status = 9; return; }

	// Calculate destination image dimensions and size
	cmd->destinationWidth  = cmd->xScale > 0 ? cmd->w * cmd->xScale : (cmd->w - cmd->xScale - 1) / -cmd->xScale;
	cmd->destinationHeight = cmd->yScale > 0 ? cmd->h * cmd->yScale : (cmd->h - cmd->yScale - 1) / -cmd->yScale;

	cmd->destinationSize = cmd->destinationWidth * cmd->destinationHeight;

	// Allocate two buffers for destination image, one for software and one for hardware scaling
	cmd->referenceImage   = malloc(sizeof(unsigned char) * cmd->destinationSize);
	cmd->destinationImage = malloc(sizeof(unsigned char) * cmd->destinationSize);

	if (cmd->referenceImage   == NULL) { cmd->status = 10; return; }
	if (cmd->destinationImage == NULL) { cmd->status = 11; return; }
}

void resizeImage(Command* cmd, HWContext* ctx)
{
	int resHW;
	int resHSCD;

	// Reset and resetart performance counter
	PERF_RESET(PERF_CNT_BASE);
	PERF_START_MEASURING(PERF_CNT_BASE);

	// Flush cache and start measuring time
	alt_dcache_flush_all();
	PERF_BEGIN(PERF_CNT_BASE, 1);

	// Run software scaler
	scaleSW(cmd->sourceImage, cmd->referenceImage, cmd->sourceWidth, cmd->sourceHeight, cmd->x, cmd->y, cmd->w, cmd->h, cmd->destinationWidth, cmd->destinationHeight, cmd->xScale, cmd->yScale);

	PERF_END(PERF_CNT_BASE, 1);

	// Flush cache and start measuring time
	alt_dcache_flush_all();
	PERF_BEGIN(PERF_CNT_BASE, 2);

	// Run hardware scaler
	scaleHW(ctx, cmd->sourceImage, cmd->destinationImage, cmd->sourceWidth, cmd->sourceHeight, cmd->x, cmd->y, cmd->w, cmd->h, cmd->destinationWidth, cmd->destinationHeight, cmd->xScale, cmd->yScale);

	PERF_END(PERF_CNT_BASE, 2);

	// Verify result
	if (checkHW(ctx)) { cmd->status = 16; return; }
	resHW = verify(cmd->referenceImage, cmd->destinationImage, cmd->destinationSize);

	// Flush cache and start measuring time
	alt_dcache_flush_all();
	PERF_BEGIN(PERF_CNT_BASE, 3);

	// Run hardware/software scaler
	scaleHSCD(ctx, cmd->sourceImage, cmd->destinationImage, cmd->sourceWidth, cmd->sourceHeight, cmd->x, cmd->y, cmd->w, cmd->h, cmd->destinationWidth, cmd->destinationHeight, cmd->xScale, cmd->yScale);

	PERF_END(PERF_CNT_BASE, 3);

	// Verify result
	if (checkHW(ctx)) { cmd->status = 16; return; }
	resHSCD = verify(cmd->referenceImage, cmd->destinationImage, cmd->destinationSize);

	// Print results
	printf("HW scaling: %s, HSCD scaling: %s\n", resHW == 0 ? "OK" : "ERR", resHSCD == 0 ? "OK" : "ERR");
	perf_print_formatted_report(PERF_CNT_BASE, ALT_CPU_CPU_FREQ, 3, "SW", "HW", "HSCD");
}

void saveImage(Command* cmd)
{
	// Prepared path to access hostfs and move to root dir
	char ffname[MAX_PATH] = "/mnt/host/../../";
	// Append entered filename
	strcat(ffname, cmd->fname);

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
	if (f == NULL) { cmd->status = 12; return; }

	// Write destinationWidth and destinationHeight
	size_t write = fwrite(&cmd->destinationWidth, sizeof(int), 1, f);
	if (write != 1) { fclose(f); cmd->status = 13; return; }

	write = fwrite(&cmd->destinationHeight, sizeof(int), 1, f);
	if (write != 1) { fclose(f); cmd->status = 14; return; }

	// Write image data to file
	write = fwrite(cmd->destinationImage, sizeof(unsigned char), cmd->destinationSize, f);
	if (write != cmd->destinationSize) { fclose(f); cmd->status = 15; return; }

	fclose(f);
}

int main()
{
	Command command;
	Command* cmd = &command;

	HWContext context;
	HWContext* ctx = &context;

	initHW(ctx);
	if (checkHW(ctx)) { return 0; }

	printHelp();

	while(1)
	{
		command = parseCommand();

		loadImage(cmd);
		CCC(cmd);
		printf("Image loaded\n");

		if (cmd->benchmark)
		{

		}
		else
		{
			prepareCommand(cmd);
			CCC(cmd);

			resizeImage(cmd, ctx);
			CCC(cmd);
			printf("Image resized\n");

			saveImage(cmd);
			CCC(cmd);
			printf("Image saved\n");
		}

		cleanup(cmd);
	}

	cleanupHW(ctx);

	return 0;
}
