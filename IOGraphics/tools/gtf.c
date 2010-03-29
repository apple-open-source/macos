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


__private_extern__ float
ratioOver( float a, float b )
{
    if( a > b)
        return( a / b );
    else
        return( b / a );
}

static int
IODisplayGetCVTSyncWidth( int horizontalActive, int verticalActive )
{
    // CVT Table 2
    enum {
        kCVTAspect4By3    = 4,
        kCVTAspect16By9   = 5,
        kCVTAspect16By10  = 6,
        kCVTAspect5By4    = 7,
        kCVTAspect15By9   = 7,
        kCVTAspectUnknown = 10
    };

    float ratio = ((float) horizontalActive) / ((float) verticalActive);

    if (ratioOver(ratio, 4.0 / 3.0) <= 1.03125)
        return (kCVTAspect4By3);

    if (ratioOver(ratio, 16.0 / 9.0) <= 1.03125)
        return (kCVTAspect16By9);

    if (ratioOver(ratio, 16.0 / 10.0) <= 1.03125)
        return (kCVTAspect16By10);

    if (ratioOver(ratio, 5.0 / 4.0) <= 1.03125)
        return (kCVTAspect5By4);

    if (ratioOver(ratio, 15.0 / 9.0) <= 1.03125)
        return (kCVTAspect15By9);

    return (kCVTAspectUnknown);
}

//FromRefreshRate
// 7.3
void GenTiming ( int requestedWidth, int requestedHeight, 
                 float frameRate, boolean_t needInterlace,
                 int genType )
{
    int         charSize = 8; 

    float       m = 600;  // % / kHz;
    float       c = 40;   // %
    float       k = 128;
    float       j = 20;   // %
    
    float       cPrime = ((c - j) * k / 256) + j;
    float       mPrime = k / 256 * m;

    float       fieldRate;                                              // V_FIELD_RATE_RQD
    // 7.
    float       interlace = needInterlace ? 0.5 : 0.0;
    float       interlaceFactor = needInterlace ? 2.0 : 1.0;

    // 1.
    fieldRate = frameRate * interlaceFactor;

    // 3.
    int         leftMargin = 0;
    int         rightMargin = 0;
    // 4.
    int         horizontalActive = roundf(requestedWidth / charSize) * charSize
                                    + leftMargin + rightMargin;         // TOTAL_ACTIVE_PIXELS 

    // 5.
    int         verticalActive = roundf(requestedHeight / interlaceFactor);     // V_LINES_RND
    // 6.
    int         topMargin = 0;
    int         bottomMargin = 0;

    int         verticalSyncWidth;                                      // V_SYNC_RND
    int         verticalSyncAndBackPorch;                               // V_SYNC_BP
    int         verticalSyncFrontPorch;
    int         horizontalTotal;                                        // TOTAL_PIXELS
    int         horizontalBlanking;                                     // H_BLANK
    int         horizontalSyncWidth;                                    // H_SYNC_PIXELS


    if (0 == genType)
        verticalSyncWidth = 3;
    else
        verticalSyncWidth = IODisplayGetCVTSyncWidth(horizontalActive, verticalActive * interlaceFactor);

    if (0 == genType)           /******************************* GTF */

    {
        float horizontalSyncPercent = 8.0/100.0;                        // H_SYNC_PER
        float verticalSyncAndBackPorchTime = 550e-6;                    // MIN_VSYNC_BP
        int   minVerticalFrontPorch = 1;                                // MIN_V_PORCH_RND
        float estimatedHorizontalPeriod;                                // H_PERIOD_EST 
        float verticalFieldTotal;                                       // TOTAL_V_LINES 
        float estimatedFieldRate;                                       // V_FIELD_RATE_EST

        // 7.
        estimatedHorizontalPeriod = 
            ((1 / fieldRate) - verticalSyncAndBackPorchTime) 
            / (verticalActive + (2 * topMargin) + minVerticalFrontPorch + interlace);

        // 8.
        verticalSyncAndBackPorch = roundf(verticalSyncAndBackPorchTime / estimatedHorizontalPeriod);
        verticalSyncFrontPorch = minVerticalFrontPorch;

//      printf("estimatedHorizontalPeriod %.9f us, verticalSyncAndBackPorch %d\n",
//                  estimatedHorizontalPeriod*1e6, verticalSyncAndBackPorch);
    
        // 10.
        verticalFieldTotal = verticalActive + topMargin + bottomMargin 
                            + verticalSyncAndBackPorch + interlace + minVerticalFrontPorch;
        // 11.
        estimatedFieldRate = 1.0 / estimatedHorizontalPeriod / verticalFieldTotal;
    
//    printf("verticalFieldTotal %.9f, estimatedFieldRate %.9f\n",
//                      verticalFieldTotal, estimatedFieldRate);
    
        // 12.
        float hPeriod = estimatedHorizontalPeriod / (fieldRate / estimatedFieldRate);
    
        printf("hPeriod %.9f us, ", hPeriod*1e6);
        printf("hFreq %.9f kHz\n", 1/hPeriod/1e3);
    
        // 18.
        float idealDutyCycle = cPrime - (mPrime * hPeriod * 1e6 / 1000.0);
        // 19.
        horizontalBlanking = 2 * charSize * roundf(
                                (horizontalActive * idealDutyCycle / (100.0 - idealDutyCycle) 
                                / (2 * charSize)));
    
//      printf("idealDutyCycle %.9f, horizontalBlanking %d\n", idealDutyCycle, horizontalBlanking);
    
        // 20.
        horizontalTotal = horizontalActive + horizontalBlanking;
        // 21.
        float pixelFreq = horizontalTotal / hPeriod;
        
        printf("pixFreq %.9f Mhz\n", pixelFreq/1e6);
    
        // gtf 2.17.
        horizontalSyncWidth = roundf(horizontalSyncPercent * horizontalTotal / charSize) * charSize;

    }
    else if (1 == genType)              /******************************* CVT */
    {
        float horizontalSyncPercent = 8.0/100.0;                        // H_SYNC_PER
        float verticalSyncAndBackPorchTime = 550e-6;                    // MIN_VSYNC_BP
        int   minVerticalBackPorch = 6;                                 // MIN_VBPORCH
        int   minVerticalFrontPorch = 3;                                // MIN_V_PORCH_RND
        float estimatedHorizontalPeriod;                                // H_PERIOD_EST 
        float verticalFieldTotal;                                       // TOTAL_V_LINES 

        // 8.
        estimatedHorizontalPeriod = 
            ((1 / fieldRate) - verticalSyncAndBackPorchTime) 
            / (verticalActive + (topMargin + bottomMargin) + minVerticalFrontPorch + interlace);
        // 9.

        verticalSyncAndBackPorch = 1 + truncf(verticalSyncAndBackPorchTime / estimatedHorizontalPeriod);
    
        if (verticalSyncAndBackPorch < (verticalSyncWidth + minVerticalBackPorch))
            verticalSyncAndBackPorch = verticalSyncWidth + minVerticalBackPorch;
    
        // 10.
//      int verticalSyncBackPorch = verticalSyncAndBackPorch - verticalSyncWidth;

        verticalSyncFrontPorch = minVerticalFrontPorch;
    
//        printf("estimatedHorizontalPeriod %.9f us, verticalSyncAndBackPorch %d\n", 
//                  estimatedHorizontalPeriod*1e6, verticalSyncAndBackPorch);
    
        // 11.
        verticalFieldTotal = verticalActive + topMargin + bottomMargin 
                            + verticalSyncAndBackPorch + interlace + minVerticalFrontPorch;
        // 12.
        float idealDutyCycle = cPrime - (mPrime * estimatedHorizontalPeriod * 1e6 / 1000.0);
    
        // 13.
        if (idealDutyCycle < 20.0)
            idealDutyCycle = 20.0;

        horizontalBlanking = 2 * charSize * truncf(
                            (horizontalActive * idealDutyCycle / (100.0 - idealDutyCycle) 
                            / (2 * charSize)));
    
        // 14.
        horizontalTotal = horizontalActive + horizontalBlanking;
    
        // 15.
        float frequencyStep = 0.25e6;                                   // CLOCK_STEP
        float pixelFrequency = frequencyStep * truncf(
                        (horizontalTotal / estimatedHorizontalPeriod) / frequencyStep);
    
        printf("pixFreq %.9f Mhz\n", pixelFrequency/1e6);
    
        // 16.
        float horizontalFrequency = pixelFrequency / horizontalTotal;
    
        printf("hPeriod %.9f us, ", (1/horizontalFrequency)*1e6);
        printf("hFreq %.9f kHz\n", horizontalFrequency/1e3);

        // gtf 2.17.
        horizontalSyncWidth = charSize * truncf(
            horizontalSyncPercent * horizontalTotal / charSize);

    }
    else        /******************************* CVT reduced blank */
    {
        float minVerticalBlankTime = 460e-6;                            // RB_MIN_V_BLANK 
        float estimatedHorizontalPeriod;                                // H_PERIOD_EST 
        int   verticalBlanking;                                         // VBI_LINES
        int   minVerticalBackPorch = 6;                                 // MIN_VBPORCH

        verticalSyncFrontPorch = 3;                                     // RB_V_FPORCH
        horizontalBlanking = 160;                                       // RB_H_BLANK
        horizontalSyncWidth = 32;                                       // RB_H_SYNC
    
        // 8.
        estimatedHorizontalPeriod = ((1 / fieldRate) - minVerticalBlankTime) 
                                    / (verticalActive + topMargin + bottomMargin);
        // 9.
        verticalBlanking = truncf(minVerticalBlankTime / estimatedHorizontalPeriod) + 1; // VBI_LINES

//        printf("estimatedHorizontalPeriod %.9f us, verticalBlanking %d\n",
//                      estimatedHorizontalPeriod*1e6, verticalBlanking);

        // 10.
        if (verticalBlanking < (verticalSyncFrontPorch + verticalSyncWidth + minVerticalBackPorch))
            verticalBlanking = (verticalSyncFrontPorch + verticalSyncWidth + minVerticalBackPorch);


        verticalSyncAndBackPorch = verticalBlanking - verticalSyncFrontPorch;

        // 11.
        int verticalFieldTotal = verticalBlanking  + verticalActive 
                                + topMargin + bottomMargin + interlace;

        // 12.
        horizontalTotal = horizontalActive + horizontalBlanking;

        // 13.
        float frequencyStep = 0.25e6;                                   // CLOCK_STEP
        float pixelFrequency = frequencyStep * truncf(
                            (horizontalTotal * verticalFieldTotal * fieldRate) / frequencyStep);

        printf("pixFreq %.9f Mhz\n", pixelFrequency/1e6);
    
        // 14.
        float horizontalFrequency = pixelFrequency / horizontalTotal;
    
        printf("hPeriod %.9f us, ", (1/horizontalFrequency)*1e6);
        printf("hFreq %.9f kHz\n", horizontalFrequency/1e3);

    }

//    printf("verticalFieldTotal %.9f, estimatedFieldRate %.9f\n",
//              verticalFieldTotal, estimatedFieldRate);
//    printf("idealDutyCycle %.9f, horizontalBlanking %d\n",
//              idealDutyCycle, horizontalBlanking);

    // 17.
//    float vFieldRate = hFreq / verticalFieldTotal;
    // 18.
//    float vFrameRate = vFieldRate / interlaceFactor;
    
    // stage 2
   
    // 3.
    int verticalTotal = interlaceFactor *
        (verticalActive + topMargin + bottomMargin 
            + verticalSyncAndBackPorch + interlace + verticalSyncFrontPorch);

    int horizontalSyncOffset = (horizontalBlanking / 2) - horizontalSyncWidth;
    // 30.
    float verticalOddBlanking = verticalSyncAndBackPorch + verticalSyncFrontPorch;
    // 32.
    float verticalEvenBlanking = verticalSyncAndBackPorch + 2 * interlace + verticalSyncFrontPorch;
    // 36.
    float verticalSyncOddFrontPorch = verticalSyncFrontPorch + interlace;

    printf("hTotal %d(%d), hFP %d(%d), hBlank %d(%d), hSync %d(%d)\n",
            horizontalTotal/8, horizontalTotal, horizontalSyncOffset/8, 
            horizontalSyncOffset, horizontalBlanking/8, horizontalBlanking, 
            horizontalSyncWidth/8, horizontalSyncWidth);

    printf("vTotal %d, vFP %.1f(E:%.1f), vBlank %.1f(E:%.1f), vSync %d\n\n",
            verticalTotal, verticalSyncOddFrontPorch, (float) verticalSyncFrontPorch, 
            verticalOddBlanking, verticalEvenBlanking, verticalSyncWidth);

}


int main (int argc, char * argv[])
{
    char * endstr;
    boolean_t needInterlace = FALSE;
    int requestedWidth = 1400, requestedHeight = 1050;
    float frameRate = 60.0;

    requestedWidth = strtol(argv[1], 0, 0);
    requestedHeight = strtol(argv[2], 0, 0);
    frameRate = strtol(argv[3], &endstr, 0);
    needInterlace = (endstr[0] == 'i') || (endstr[0] == 'I');

    printf("\nGTF:\n\n");
    GenTiming( requestedWidth, requestedHeight, frameRate, needInterlace, 0 );
    printf("\nCVT:\n\n");
    GenTiming( requestedWidth, requestedHeight, frameRate, needInterlace, 1 );
    printf("\nCVT reduced blank:\n\n");
    GenTiming( requestedWidth, requestedHeight, frameRate, needInterlace, 2 );

    return(0);
}

