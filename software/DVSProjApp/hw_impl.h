#ifndef HW_IMPL_H_
#define HW_IMPL_H_

void printHWError(int status);
void cleanupHW(int status);
int checkHWStatus(int status);
int initHW();
int scaleHW(unsigned char* source, unsigned char* destination, int sourceWidth, int sourceHeight, int x, int y, int width, int height, int xScale, int yScale);
int scaleHSCD(unsigned char* source, unsigned char* destination, int sourceWidth, int sourceHeight, int x, int y, int width, int height, int xScale, int yScale);

#endif /* HW_IMPL_H_ */
