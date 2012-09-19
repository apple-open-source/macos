/*
cc -o /tmp/bootgamma bootgamma.c -framework IOKit -framework CoreFoundation
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <assert.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <CoreFoundation/CoreFoundation.h>




void IOFBDecompressGamma(const IOFBBootGamma * bootGamma, uint16_t * data, uint16_t count)
{
	const IOFBGamma * channelGamma;
	uint16_t channel, idx, maxIdx, seg;
	uint16_t startIn, startOut;
	uint16_t endIn, endOut;

	maxIdx = count - 1;
	channelGamma = &bootGamma->gamma.red;
	for (channel = 0; channel < 3; channel++)
	{
		seg = 0;
		startIn = 0;
		startOut = 0x0000;
		endIn = 0;
		endOut = 0;
		for (idx = 0; idx <= maxIdx; idx++)
		{
			if ((idx == endIn) && (idx != maxIdx))
			{
				startIn = endIn;
				startOut = endOut;
				if (seg < channelGamma->pointCount)
				{
					endIn = (channelGamma->points[seg].in * count) >> 16;
					endOut = channelGamma->points[seg].out;
					seg++;
				}
				else
				{
					endIn = maxIdx;
					endOut = 0xFFFF;
				}
			}
			data[channel * count + idx] = startOut + ((endOut - startOut) * (idx - startIn)) / (endIn - startIn);
		}
		channelGamma = (typeof(channelGamma)) &channelGamma->points[channelGamma->pointCount];
	}
}

int main(int argc, char * argv[])
{
	IOFBBootGamma * bootGamma;
	io_registry_entry_t regEntry;
	CFDataRef data = NULL;
	const uint8_t * bytes;
	const uint8_t * endData;

	regEntry = IORegistryEntryFromPath(kIOMasterPortDefault, 
                                    kIODeviceTreePlane ":/options");
	if (!regEntry) exit(1);
	data = IORegistryEntryCreateCFProperty(regEntry, CFSTR(kIOFBBootGammaKey),
									kCFAllocatorDefault, kNilOptions);
	if (!data) exit(2);

	endData = bytes = CFDataGetBytePtr(data);
	endData += CFDataGetLength(data);

	uint16_t * deco;
	uint16_t count = 256;
	deco = (typeof(deco)) malloc(sizeof(uint16_t) * 3 * count);

	while (bytes < endData)
	{
		bootGamma = (typeof(bootGamma)) bytes;
		bytes += bootGamma->length;
		assert (bytes <= endData);

		fprintf(stderr, "\ntable vend 0x%x, prod 0x%x, serial 0x%x, length 0x%x\n",
				bootGamma->vendor, bootGamma->product, bootGamma->serial,
				bootGamma->length);

		IOFBDecompressGamma(bootGamma, deco, count);

		uint16_t idx;
		for (idx = 0; idx < count; idx++)
		{
			printf("0x%04x 0x%04x 0x%04x\n", 
					deco[0 * count + idx], deco[1 * count + idx], deco[2 * count + idx]);
		}
	}
	CFRelease(data);
	free(deco);
	IOObjectRelease(regEntry);

	return (0);
}


