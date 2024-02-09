#include "hw_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <system.h>
#include <altera_avalon_sgdma_regs.h>

// Memory Map
#define CR_ADDR 0
#define WH_ADDR 4

// Control and Status Register Map
#define X_SCALE_OFFSET 0
#define X_UPSCALE_OFFSET 2
#define Y_SCALE_OFFSET 3
#define Y_UPSCALE_OFFSET 5

// Width and Height Register Map
#define WIDTH_OFFSET 0
#define HEIGHT_OFFSET 16

// Line buffer size, defined in hardware
#define BUFFER_SIZE 1024

// Macro to calculate index in row linearized matrix from coordinates
#define PIXEL(x, y, width) ((x) + (y) * (width))

// SGDMA Transmit Complete callback
void txCallback(void* ctx)
{
	((HWContext*)ctx)->txDone++;
}

// SGDMA Receive Complete callback
void rxCallback(void* ctx)
{
	((HWContext*)ctx)->rxDone++;
}

void printHWError(HWContext* ctx)
{
	int status = ctx->status;
	if (status == 0)                     { printf("No error\n"); }
	else if (status == 1)                { printf("Failed to open tx SGDMA\n"); }
	else if (status == 2)                { printf("Failed to open rx SGDMA\n"); }
	else if (status == 3)                { printf("Failed to allocate descriptors\n"); }
	else if (status == 4)                { printf("Failed to start tx SGDMA\n"); }
	else if (status == 5)                { printf("Failed to start rx SGDMA\n"); }
	else if (status == 6 || status == 7) { printf("Invalid image size for hardware scaling\n"); }
	else                                 { printf("Unknown error\n"); }
}

void cleanupHW(HWContext* ctx)
{
	if (ctx->mallocPtr != NULL) { free(ctx->mallocPtr); }
}

int checkHW(HWContext* ctx)
{
	if (ctx->status != 0)
	{
		printHWError(ctx);
		cleanupHW(ctx);
		return 1;
	}
	return 0;
}

void initHW(HWContext* ctx)
{
	ctx->status = 0;

	// Open tx and rx SGDMA
	ctx->txHandle = alt_avalon_sgdma_open(SGDMA_M2S_NAME);
	if (ctx->txHandle == NULL) { ctx->status = 1; return; }

	ctx->rxHandle = alt_avalon_sgdma_open(SGDMA_S2M_NAME);
	if (ctx->rxHandle == NULL) { ctx->status = 2; return; }

	// Allocate descriptors for maximum possible image size
	// Maximum input image size is BUFFER_SIZE * BUFFER_SIZE pixels
	// Maximum output image size is 4 * BUFFER_SIZE * 4 * BUFFER_SIZE pixels
	// With one descriptor for each line that is 5 * BBUFFER_SIZE, + 2 stop descriptors, + 1 descriptor for alignment
	// Which is close enough to (BUFFER_SIZE + 1) * 5
	ctx->mallocPtr = malloc(((BUFFER_SIZE + 1) * 5) * ALTERA_AVALON_SGDMA_DESCRIPTOR_SIZE);
	if (ctx->mallocPtr == NULL) { ctx->status = 3; return; }

	// Zero log2(ALTERA_AVALON_SGDMA_DESCRIPTOR_SIZE) lsbs to guarantee alignment
	// If that address is outside the allocated memory, increment descPtr
	ctx->descPtr = (alt_sgdma_descriptor*)((alt_u32)ctx->mallocPtr & ~(ALTERA_AVALON_SGDMA_DESCRIPTOR_SIZE - 1));
	if (ctx->descPtr < ctx->mallocPtr) { ctx->descPtr++; }

	// Register tx and rx callbacks
	alt_u32 controlMask = (ALTERA_AVALON_SGDMA_CONTROL_IE_GLOBAL_MSK | ALTERA_AVALON_SGDMA_CONTROL_IE_CHAIN_COMPLETED_MSK | ALTERA_AVALON_SGDMA_CONTROL_PARK_MSK);
	alt_avalon_sgdma_register_callback(ctx->txHandle, txCallback, controlMask, ctx);
	alt_avalon_sgdma_register_callback(ctx->rxHandle, rxCallback, controlMask, ctx);
}

void scaleHW(HWContext* ctx, unsigned char* source, unsigned char* destination, int sourceWidth, int sourceHeight, int x, int y, int width, int height, int destinationWidth, int destinationHeight, int xScale, int yScale)
{
	int descIdx = 0;

	// Check image size
	if (width  > BUFFER_SIZE) { ctx->status = 6; return; }
	if (height > BUFFER_SIZE) { ctx->status = 7; return; }

	// Encode scaling factor
	int xUpscale = (xScale > 0);
	int yUpscale = (yScale > 0);
	xScale = xScale > 0 ? xScale - 1 : -xScale - 1;
	yScale = yScale > 0 ? yScale - 1 : -yScale - 1;

	// Write memory-mapped registers
	alt_u32 cr = yUpscale << Y_UPSCALE_OFFSET | yScale << Y_SCALE_OFFSET | xUpscale << X_UPSCALE_OFFSET | xScale << X_SCALE_OFFSET;
	alt_u32 wh = height << HEIGHT_OFFSET | width << WIDTH_OFFSET;
	IOWR_32DIRECT(ACC_SCALE_BASE, CR_ADDR, cr);
	IOWR_32DIRECT(ACC_SCALE_BASE, WH_ADDR, wh);

	// Start using descriptors for tx from the beginning
	alt_sgdma_descriptor* txDesc = &(ctx->descPtr[descIdx]);
	for (int i = 0; i < height; i++)
	{
		// Construct descriptor for each source line
		alt_avalon_sgdma_construct_mem_to_stream_desc(&(ctx->descPtr[descIdx]), &(ctx->descPtr[descIdx + 1]), (alt_u32*)&source[PIXEL(x, y + i, sourceWidth)], width, 0, 0, 0, 0);
		descIdx++;
	}
	// Set next descriptor as stop descriptor
	ctx->descPtr[descIdx++].control = 0;

	// Start using descriptors for rx right after tx stop descriptor
	alt_sgdma_descriptor* rxDesc = &(ctx->descPtr[descIdx]);
	for (int i = 0; i < destinationHeight; i++)
	{
		// Construct descriptor for each destination line
		alt_avalon_sgdma_construct_stream_to_mem_desc(&(ctx->descPtr[descIdx]), &(ctx->descPtr[descIdx + 1]), (alt_u32*)&destination[PIXEL(0, i, destinationWidth)], destinationWidth, 0);
		descIdx++;
	}
	// Set next descriptor as stop descriptor
	ctx->descPtr[descIdx++].control = 0;

	// Reset completion flags
	ctx->txDone = 0;
	ctx->rxDone = 0;

	// Start tx and rx SGDMA
	if (alt_avalon_sgdma_do_async_transfer(ctx->txHandle, txDesc)) { ctx->status = 4; return; }
	if (alt_avalon_sgdma_do_async_transfer(ctx->rxHandle, rxDesc)) { ctx->status = 5; return; }

	// Wait for completion
	while (ctx->txDone == 0 || ctx->rxDone == 0) {}

	// Stop tx and rx SGDMA
	alt_avalon_sgdma_stop(ctx->txHandle);
	alt_avalon_sgdma_stop(ctx->rxHandle);
}

void scaleHSCD(HWContext* ctx, unsigned char* source, unsigned char* destination, int sourceWidth, int sourceHeight, int x, int y, int width, int height, int destinationWidth, int destinationHeight, int xScale, int yScale)
{
	int descIdx = 0;

	// Check image size
	if (width  > BUFFER_SIZE) { ctx->status = 6; return; }
	if (height > BUFFER_SIZE) { ctx->status = 7; return; }

	// Encode scaling factor
	int xUpscale = (xScale > 0);
	int yUpscale = (yScale > 0);
	xScale = xScale > 0 ? xScale - 1 : -xScale - 1;
	yScale = yScale > 0 ? yScale - 1 : -yScale - 1;

	// Start using descriptors for tx from the beginning
	alt_sgdma_descriptor* txDesc = &(ctx->descPtr[descIdx]);
	if (yUpscale)
	{
		for (int i = 0; i < height; i++)
		{
			// If upscaling construct descriptors as usual
			alt_avalon_sgdma_construct_mem_to_stream_desc(&(ctx->descPtr[descIdx]), &(ctx->descPtr[descIdx + 1]), (alt_u32*)&source[PIXEL(x, y + i, sourceWidth)], width, 0, 0, 0, 0);
			descIdx++;
		}
	}
	else
	{
		for (int i = 0; i < height; i+= yScale + 1)
		{
			// If downscaling construct descriptors only for each yScale-th source line
			alt_avalon_sgdma_construct_mem_to_stream_desc(&(ctx->descPtr[descIdx]), &(ctx->descPtr[descIdx + 1]), (alt_u32*)&source[PIXEL(x, y + i, sourceWidth)], width, 0, 0, 0, 0);
			descIdx++;
		}
		// Since extra lines are not transmitted yScale is 1 (encoded as 0) and height is the same as destinationHeight
		yScale = 0;
		height = destinationHeight;
	}
	// Set next descriptor as stop descriptor
	ctx->descPtr[descIdx++].control = 0;

	// Start using descriptors for rx right after tx stop descriptor
	alt_sgdma_descriptor* rxDesc = &(ctx->descPtr[descIdx]);
	for (int i = 0; i < destinationHeight; i++)
	{
		// Rx descriptors are same as usual
		alt_avalon_sgdma_construct_stream_to_mem_desc(&(ctx->descPtr[descIdx]), &(ctx->descPtr[descIdx + 1]), (alt_u32*)&destination[PIXEL(0, i, destinationWidth)], destinationWidth, 0);
		descIdx++;
	}
	// Set next descriptor as stop descriptor
	ctx->descPtr[descIdx++].control = 0;

	// Write memory-mapped registers here since yScale and height may change after descriptor construction
	alt_u32 cr = yUpscale << Y_UPSCALE_OFFSET | yScale << Y_SCALE_OFFSET | xUpscale << X_UPSCALE_OFFSET | xScale << X_SCALE_OFFSET;
	alt_u32 wh = height << HEIGHT_OFFSET | width << WIDTH_OFFSET;
	IOWR_32DIRECT(ACC_SCALE_BASE, CR_ADDR, cr);
	IOWR_32DIRECT(ACC_SCALE_BASE, WH_ADDR, wh);

	// Reset completion flags
	ctx->txDone = 0;
	ctx->rxDone = 0;

	// Start tx and rx SGDMA
	if (alt_avalon_sgdma_do_async_transfer(ctx->txHandle, txDesc)) { ctx->status = 4; return; }
	if (alt_avalon_sgdma_do_async_transfer(ctx->rxHandle, rxDesc)) { ctx->status = 5; return; }

	// Wait for completion
	while (ctx->txDone == 0 || ctx->rxDone == 0) {}

	// Stop tx and rx SGDMA
	alt_avalon_sgdma_stop(ctx->txHandle);
	alt_avalon_sgdma_stop(ctx->rxHandle);
}
