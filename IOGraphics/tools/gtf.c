// cc -o /tmp/gtf -g gtf.c -Wall


#include <mach/mach.h>
#include <mach/thread_switch.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//FromRefreshRate
// 7.3
int main (int argc, char * argv[])
{
    char * endstr;
    boolean_t needInterlace = FALSE;
    int hPixels = 1376, vLines = 774;
    float vFrameRateRqd = 60.0;

    //

    int charSize = 1; 
    int vSyncRqd = 3;
    float hSyncPct = 8.0/100.0; // %
    float minVSyncBP = 550e-6; // s
    int minPorchRnd = 1;

    float m = 600;  // % / kHz;
    float c = 40;   // %
    float k = 128;
    float j = 20;   // %
    
    float cPrime = ((c - j) * k / 256) + j;
    float mPrime = k / 256 * m;

    //

    hPixels = strtol(argv[1], 0, 0);
    vLines = strtol(argv[2], 0, 0);
    vFrameRateRqd = strtol(argv[3], &endstr, 0);
    needInterlace = (endstr[0] == 'i') || (endstr[0] == 'I');

    //

    float vFieldRateRqd;
    float interlace = needInterlace ? 0.5 : 0.0;
    float interlaceFactor = needInterlace ? 2.0 : 1.0;

    //

    vFieldRateRqd = vFrameRateRqd * interlaceFactor;

    // 1.
    int hPixelsRnd = roundf(hPixels / charSize) * charSize;
    int vLinesRnd = roundf( vLines / interlaceFactor );
    
    int topMargin = 0;
    int bottomMargin = 0;

    // 7.
    float hPeriodEst = ((1 / vFieldRateRqd) - (minVSyncBP)) 
		     / (vLinesRnd + (2 * topMargin) + minPorchRnd + interlace);
    // 8.
    int vSyncBP = roundf( minVSyncBP / hPeriodEst );

//    printf("hPeriodEst %.9f us, vSyncBP %d\n", hPeriodEst*1e6, vSyncBP);

    // 10.
    float totalVLines = vLinesRnd + topMargin + bottomMargin + vSyncBP + interlace + minPorchRnd;
    // 11.
    float vFieldRateEst = 1 / hPeriodEst / totalVLines;

//    printf("totalVLines %.9f, vFieldRateEst %.9f\n", totalVLines, vFieldRateEst);

    // 12.
    float hPeriod = hPeriodEst / (vFieldRateRqd / vFieldRateEst);

    printf("hPeriod %.9f us, ", hPeriod*1e6);
    printf("hFreq %.9f kHz\n", 1/hPeriod/1e3);

    // 15.
    int leftMargin = 0;
    int rightMargin = 0;
    // 17.
    int totalActivePixels = hPixelsRnd + leftMargin + rightMargin;
    // 18.
    float idealDutyCycle = cPrime - (mPrime * hPeriod * 1e6 / 1000.0);
    // 19.
    int hBlankPixels = roundf((totalActivePixels * idealDutyCycle / (100.0 - idealDutyCycle) / (2 * charSize))) * 2 * charSize;

//    printf("idealDutyCycle %.9f, hBlankPixels %d\n", idealDutyCycle, hBlankPixels);

    // 20.
    int totalPixels = totalActivePixels + hBlankPixels;
    // 21.
    float pixelFreq = totalPixels / hPeriod;
    
    printf("pixFreq %.9f Mhz\n", pixelFreq/1e6);

    // stage 2
   
    // 3.
    int totalLines = interlaceFactor * (vLinesRnd + topMargin + bottomMargin + vSyncBP + interlace + minPorchRnd);

    // 17.
    int hSyncPixels = roundf( hSyncPct * totalPixels / charSize) * charSize;
    int hFPPixels = (hBlankPixels / 2) - hSyncPixels;
    // 30.
    float vOddBlankingLines = vSyncBP + minPorchRnd;
    // 32.
    float vEvenBlankingLines = vSyncBP + 2*interlace + minPorchRnd;
    // 36.
    float vOddFPLines = minPorchRnd + interlace;

    printf("hTotal %d(%d), hFP %d(%d), hBlank %d(%d), hSync %d(%d)\n", totalPixels/8, totalPixels, 
	    hFPPixels/8, hFPPixels, hBlankPixels/8, hBlankPixels, hSyncPixels/8, hSyncPixels);

    printf("vTotal %d, vFP %.1f(E:%.1f), vBlank %.1f(E:%.1f), vSync %d\n",
	totalLines, vOddFPLines, (float) minPorchRnd, vOddBlankingLines, vEvenBlankingLines, vSyncRqd);

    return(0);
}
