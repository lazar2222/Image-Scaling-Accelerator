#include "sw_impl.h"

#include <string.h>

// Macro to calculate index in row linearized matrix from coordinates
#define PIXEL(x, y, width) ((x) + (y) * (width))

void scaleLineSW(unsigned char* source, unsigned char* destination, int width, int xScale)
{
	if (xScale > 0)
	{
		// For each source pixel, write it to destination xScale times
		for (int i = 0; i < width; i++)
		{
			// Using memset in hopes of compiler optimization
			memset(&destination[i * xScale], source[i], xScale);
		}
	}
	else
	{
		// When downscaling we have to negate the scaling factor
		xScale = - xScale;

		// For each xScale-th source pixel, write it to destination in consecutive locations
		for (int i = 0; i < width; i += xScale)
		{
			destination[i / xScale] = source[i];
		}
	}
}

void scaleSW(unsigned char* source, unsigned char* destination, int sourceWidth, int sourceHeight, int x, int y, int width, int height, int destinationWidth, int destinationHeight, int xScale, int yScale)
{
	if (yScale > 0)
	{
		// For each source line, scale it and write it to destination yScale times
		for (int i = 0; i < height; i++)
		{
			// We scale the first line manually and memcpy it remaining yScale - 1 times via fall-through switch statement similar to those used when loop unrolling, again in hopes of compiler optimization
			scaleLineSW(&source[PIXEL(x, y + i, sourceWidth)], &destination[PIXEL(0, i * yScale, destinationWidth)], width, xScale);
			switch (yScale)
			{
			case 4: { memcpy(&destination[PIXEL(0, i * yScale + 3, destinationWidth)], &destination[PIXEL(0, i * yScale, destinationWidth)], destinationWidth); }
			case 3: { memcpy(&destination[PIXEL(0, i * yScale + 2, destinationWidth)], &destination[PIXEL(0, i * yScale, destinationWidth)], destinationWidth); }
			case 2: { memcpy(&destination[PIXEL(0, i * yScale + 1, destinationWidth)], &destination[PIXEL(0, i * yScale, destinationWidth)], destinationWidth); }
			default: { break; }
			}
		}
	}
	else
	{
		// When downscaling we have to negate the scaling factor
		yScale = - yScale;

		// For each yScale-th source line, scale it and write it to destination in consecutive locations
		for (int i = 0; i < height; i += yScale)
		{
			scaleLineSW(&source[PIXEL(x, y + i, sourceWidth)], &destination[PIXEL(0, i / yScale , destinationWidth)], width, xScale);
		}
	}
}

