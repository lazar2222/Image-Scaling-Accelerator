#include "sw_impl.h"

#include <string.h>

#define PIXEL(x, y, width) ((x) + (y) * (width))

void scaleLineSW(unsigned char* source, unsigned char* destination, int width, int xScale)
{
	if (xScale > 0)
	{
		for (int i = 0; i < width; i++)
		{
			memset(&destination[i * xScale], source[i], xScale);
		}
	}
	else
	{
		xScale = - xScale;
		for (int i = 0; i < width; i += xScale)
		{
			destination[i / xScale] = source[i];
		}
	}
}

void scaleSW(unsigned char* source, unsigned char* destination, int sourceWidth, int sourceHeight, int x, int y, int width, int height, int xScale, int yScale)
{
	int dwidth = xScale > 0 ? width * xScale : width / -xScale;
	if (yScale > 0)
	{
		for (int i = 0; i < height; i++)
		{
			scaleLineSW(&source[PIXEL(x, y + i, sourceWidth)], &destination[PIXEL(0, i * yScale, dwidth)], width, xScale);
			switch(yScale)
			{
			case 4: { memcpy(&destination[PIXEL(0, i * yScale + 3, dwidth)], &destination[PIXEL(0, i * yScale, dwidth)], dwidth); }
			case 3: { memcpy(&destination[PIXEL(0, i * yScale + 2, dwidth)], &destination[PIXEL(0, i * yScale, dwidth)], dwidth); }
			case 2: { memcpy(&destination[PIXEL(0, i * yScale + 1, dwidth)], &destination[PIXEL(0, i * yScale, dwidth)], dwidth); }
			default : { break; }
			}
		}
	}
	else
	{
		yScale = - yScale;
		for (int i = 0; i < height; i += yScale)
		{
			scaleLineSW(&source[PIXEL(x, y + i, sourceWidth)], &destination[PIXEL(0, i / yScale , dwidth)], width, xScale);
		}
	}
}

