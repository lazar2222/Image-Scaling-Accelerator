#include "hw_impl.h"

#include <stdio.h>
#include <stdlib.h>
#include <system.h>
#include <altera_avalon_sgdma.h>
#include <altera_avalon_sgdma_regs.h>

#define CR_ADDR 0
#define WH_ADDR 4

#define X_SCALE_OFFSET 0
#define X_UPSCALE_OFFSET 2
#define Y_SCALE_OFFSET 3
#define Y_UPSCALE_OFFSET 5
#define WIDTH_OFFSET 0
#define HEIGHT_OFFSET 16

#define BUFFER_SIZE 1024
#define PIXEL(x, y, width) ((x) + (y) * (width))

alt_sgdma_dev* sgdma_tx = NULL;
alt_sgdma_dev* sgdma_rx = NULL;

alt_sgdma_descriptor* bufferPtr = NULL;
alt_sgdma_descriptor* descPtr = NULL;

volatile alt_32 tx_done = 0;
volatile alt_32 rx_done = 0;

void txCallback(void * ctx)
{
	alt_32* tx_done = ctx;
	(*tx_done)++;
}

void rxCallback(void * ctx)
{
	alt_32* rx_done = ctx;
	(*rx_done)++;
}

void printHWError(int status)
{
	if (status == 0) { printf("No error\n"); }
	else if (status == 1) { printf("Failed to open tx SGDMA\n"); }
	else if (status == 2) { printf("Failed to open rx SGDMA\n"); }
	else if (status == 3) { printf("Failed to allocate descriptors\n"); }
	else { printf("Unknown error\n"); }
}

void cleanupHW(int status)
{
	if (bufferPtr != NULL) { free(bufferPtr); }
}

int checkHWStatus(int status)
{
	if (status != 0)
	{
		printHWError(status);
		cleanupHW(status);
		return 1;
	}
	return 0;
}

int initHW()
{
	sgdma_tx = alt_avalon_sgdma_open(SGDMA_M2S_NAME);
	if (sgdma_tx == NULL) {return 1;}

	sgdma_rx = alt_avalon_sgdma_open(SGDMA_S2M_NAME);
	if (sgdma_rx == NULL) {return 2;}

	bufferPtr = malloc(((BUFFER_SIZE + 1) * 5) * ALTERA_AVALON_SGDMA_DESCRIPTOR_SIZE);
	if (bufferPtr == NULL) {return 3;}

	descPtr = (alt_sgdma_descriptor*)((alt_32)bufferPtr & ~(ALTERA_AVALON_SGDMA_DESCRIPTOR_SIZE - 1));
	if (descPtr < bufferPtr) { descPtr++; }

	alt_u32 controlMask = (ALTERA_AVALON_SGDMA_CONTROL_IE_GLOBAL_MSK | ALTERA_AVALON_SGDMA_CONTROL_IE_CHAIN_COMPLETED_MSK | ALTERA_AVALON_SGDMA_CONTROL_PARK_MSK);
	alt_avalon_sgdma_register_callback(sgdma_tx, txCallback, controlMask, &tx_done);
	alt_avalon_sgdma_register_callback(sgdma_rx, rxCallback, controlMask, &rx_done);

	return 0;
}

int scaleHW(unsigned char* source, unsigned char* destination, int sourceWidth, int sourceHeight, int x, int y, int width, int height, int xScale, int yScale)
{
	int dwidth = xScale > 0 ? width * xScale : width / -xScale;
	int dheight = yScale > 0 ? height * yScale : height / -yScale;

	int descIdx = 0;

	alt_sgdma_descriptor* tx_desc = &descPtr[descIdx];
	for(int i = 0; i < height; i++)
	{
		alt_avalon_sgdma_construct_mem_to_stream_desc(&descPtr[descIdx], &descPtr[descIdx + 1], (alt_u32*)&source[PIXEL(x, y + i, sourceWidth)], width, 0, 0, 0, 0);
		descIdx++;
	}
	descPtr[descIdx++].control = 0;

	alt_sgdma_descriptor* rx_desc = &descPtr[descIdx];
	for(int i = 0; i < dheight; i++)
	{
		alt_avalon_sgdma_construct_stream_to_mem_desc(&descPtr[descIdx], &descPtr[descIdx + 1], (alt_u32*)&destination[PIXEL(0, i, dwidth)], dwidth, 0);
		descIdx++;
	}
	descPtr[descIdx++].control = 0;

	alt_u32 cr = (xScale > 0) << X_UPSCALE_OFFSET | (yScale > 0) << Y_UPSCALE_OFFSET;
	xScale = xScale > 0 ? xScale : -xScale;
	yScale = yScale > 0 ? yScale : -yScale;
	cr |= (xScale - 1) << X_SCALE_OFFSET | (yScale - 1) << Y_SCALE_OFFSET;
	IOWR_32DIRECT(UP_DOWN_SCALER_BASE, CR_ADDR, cr);
	alt_u32 wh = width << WIDTH_OFFSET | height << HEIGHT_OFFSET;
	IOWR_32DIRECT(UP_DOWN_SCALER_BASE, WH_ADDR, wh);

	if(alt_avalon_sgdma_do_async_transfer(sgdma_tx, tx_desc)) { return 4; }
	if(alt_avalon_sgdma_do_async_transfer(sgdma_rx, rx_desc)) { return 5; }

	while(tx_done == 0 || rx_done == 0) {}

	tx_done = 0;
	rx_done = 0;

	alt_avalon_sgdma_stop(sgdma_tx);
	alt_avalon_sgdma_stop(sgdma_rx);

	return 0;
}

int scaleHSCD(unsigned char* source, unsigned char* destination, int sourceWidth, int sourceHeight, int x, int y, int width, int height, int xScale, int yScale)
{
	return 0;
}
