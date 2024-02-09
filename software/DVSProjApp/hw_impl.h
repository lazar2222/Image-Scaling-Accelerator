#ifndef HW_IMPL_H_
#define HW_IMPL_H_

#include <altera_avalon_sgdma.h>

typedef struct
{
	int status;
	alt_sgdma_dev* txHandle;
	alt_sgdma_dev* rxHandle;
	alt_sgdma_descriptor* mallocPtr;
	alt_sgdma_descriptor* descPtr;
	volatile alt_32 txDone;
	volatile alt_32 rxDone;
} HWContext;

void printHWError(HWContext* ctx);
void cleanupHW(HWContext* ctx);
int checkHW(HWContext* ctx);
void initHW(HWContext* ctx);
void scaleHW(HWContext* ctx, unsigned char* source, unsigned char* destination, int sourceWidth, int sourceHeight, int x, int y, int width, int height, int destinationWidth, int destinationHeight, int xScale, int yScale);
void scaleHSCD(HWContext* ctx, unsigned char* source, unsigned char* destination, int sourceWidth, int sourceHeight, int x, int y, int width, int height, int destinationWidth, int destinationHeight, int xScale, int yScale);

#endif /* HW_IMPL_H_ */
